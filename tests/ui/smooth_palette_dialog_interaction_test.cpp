// Real-widget interaction tests for SmoothPaletteDialog (src/ui/
// SmoothPaletteDialog.h) - the underlying algorithm (core/edit/
// SmoothPalette.h) is already covered independently in
// smooth_palette_test.cpp; this file verifies the Qt wiring on top of it:
// controls start disabled with no image loaded, opening a real image file
// generates and previews a palette automatically, Apply emits a ColorMap
// that's actually sampled from the opened image (not a placeholder),
// Generate produces a different result on the same image, and a
// missing/unreadable file leaves the dialog in a clean, disabled state
// rather than crashing.

#include <cstdio>

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTest>

#include "../TestHelpers.h"
#include "SmoothPaletteDialog.h"

using apo_test::check;

namespace {

// A small, colorful test image written to disk - varied colors (not just
// 1-2 flat ones) so sampled/reordered palettes are meaningfully
// distinguishable from each other and from "no real data at all".
QString writeTestImage(const QString& path) {
    QImage image(16, 16, QImage::Format_RGB888);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            image.setPixelColor(x, y, QColor((x * 37) % 256, (y * 91) % 256, ((x + y) * 53) % 256));
        }
    }
    image.save(path, "PNG");
    return path;
}

bool colorMapEntryComesFromImage(const apo::ColorMap& cmap, int index, const QImage& image) {
    const auto& e = cmap.entries[static_cast<size_t>(index)];
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.red() == e[0] && c.green() == e[1] && c.blue() == e[2]) return true;
        }
    }
    return false;
}

void testControlsStartDisabledWithNoImage() {
    auto* dialog = new apo::ui::SmoothPaletteDialog();
    auto* pathLabel = dialog->findChild<QLabel*>("pathLabel");
    auto* generateButton = dialog->findChild<QPushButton*>("generateButton");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(pathLabel && generateButton && applyButton, "controls found")) {
        delete dialog;
        return;
    }

    check(pathLabel->text() == "No image chosen", "the path label starts with a clear empty-state message");
    check(!generateButton->isEnabled(), "Generate starts disabled with no image loaded");
    check(!applyButton->isEnabled(), "Apply starts disabled with no image loaded");

    delete dialog;
}

void testOpeningAnImageGeneratesAndPreviewsAutomatically() {
    const QString path = writeTestImage("smooth_palette_dialog_test_image.png");

    auto* dialog = new apo::ui::SmoothPaletteDialog();
    auto* pathLabel = dialog->findChild<QLabel*>("pathLabel");
    auto* generateButton = dialog->findChild<QPushButton*>("generateButton");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    auto* previewStrip = dialog->findChild<QLabel*>("previewStrip");
    if (!check(pathLabel && generateButton && applyButton && previewStrip, "controls found")) {
        delete dialog;
        return;
    }

    dialog->openImageFile(path);
    check(pathLabel->text() == path, "the path label shows the opened file's path");
    check(generateButton->isEnabled(), "Generate becomes enabled once an image is loaded");
    check(applyButton->isEnabled(), "Apply becomes enabled automatically - opening an image runs one Generate pass");
    check(!previewStrip->pixmap().isNull(), "the preview strip shows a real palette pixmap after opening an image");

    delete dialog;
    std::remove(path.toStdString().c_str());
}

void testApplyEmitsAColorMapSampledFromTheOpenedImage() {
    const QString path = writeTestImage("smooth_palette_dialog_apply_test_image.png");
    const QImage sourceImage(path);

    auto* dialog = new apo::ui::SmoothPaletteDialog();
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(applyButton != nullptr, "applyButton found")) {
        delete dialog;
        return;
    }

    dialog->openImageFile(path);

    // QSignalSpy needs its carried type registered as a Qt metatype -
    // apo::ColorMap isn't (matching GradientBrowserDialog's own identical
    // ColorMap-carrying signal, whose own interaction test uses this same
    // direct-connect-and-capture-by-reference approach instead, for the
    // same reason) - so this connects directly rather than via QSignalSpy.
    apo::ColorMap received{};
    bool receivedSignal = false;
    QObject::connect(dialog, &apo::ui::SmoothPaletteDialog::paletteApplied, dialog, [&](apo::ColorMap cmap) {
        received = cmap;
        receivedSignal = true;
    });
    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(receivedSignal, "clicking Apply emits paletteApplied");
    if (receivedSignal) {
        bool everyEntryFromImage = true;
        for (int i = 0; i < 256 && everyEntryFromImage; ++i) {
            if (!colorMapEntryComesFromImage(received, i, sourceImage)) everyEntryFromImage = false;
        }
        check(everyEntryFromImage,
              "every entry of the emitted ColorMap is an exact color sampled from the opened image, proving the "
              "whole Browse -> algorithm -> Apply pipeline is wired correctly, not just the core function alone");
    }

    delete dialog;
    std::remove(path.toStdString().c_str());
}

void testGenerateProducesADifferentResultOnTheSameImage() {
    const QString path = writeTestImage("smooth_palette_dialog_generate_test_image.png");

    auto* dialog = new apo::ui::SmoothPaletteDialog();
    auto* generateButton = dialog->findChild<QPushButton*>("generateButton");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(generateButton && applyButton, "controls found")) {
        delete dialog;
        return;
    }

    dialog->openImageFile(path);

    apo::ColorMap lastReceived{};
    int receivedCount = 0;
    QObject::connect(dialog, &apo::ui::SmoothPaletteDialog::paletteApplied, dialog, [&](apo::ColorMap cmap) {
        lastReceived = cmap;
        ++receivedCount;
    });

    QTest::mouseClick(applyButton, Qt::LeftButton);
    if (!check(receivedCount == 1, "first Apply emits once")) {
        delete dialog;
        return;
    }
    const apo::ColorMap cmap1 = lastReceived;

    QTest::mouseClick(generateButton, Qt::LeftButton);
    QTest::mouseClick(applyButton, Qt::LeftButton);
    if (!check(receivedCount == 2, "second Apply (after Generate) emits once more")) {
        delete dialog;
        return;
    }
    const apo::ColorMap cmap2 = lastReceived;

    check(!(cmap1.entries == cmap2.entries),
          "Generate re-samples with a new random seed, producing a different palette from the same image");

    delete dialog;
    std::remove(path.toStdString().c_str());
}

void testOpeningAMissingFileLeavesControlsDisabled() {
    auto* dialog = new apo::ui::SmoothPaletteDialog();
    auto* pathLabel = dialog->findChild<QLabel*>("pathLabel");
    auto* generateButton = dialog->findChild<QPushButton*>("generateButton");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(pathLabel && generateButton && applyButton, "controls found")) {
        delete dialog;
        return;
    }

    dialog->openImageFile("this_file_does_not_exist_12345.png");
    check(pathLabel->text().contains("failed to load"), "the path label reports the load failure");
    check(!generateButton->isEnabled(), "Generate stays disabled after a failed load");
    check(!applyButton->isEnabled(), "Apply stays disabled after a failed load");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testControlsStartDisabledWithNoImage();
    testOpeningAnImageGeneratesAndPreviewsAutomatically();
    testApplyEmitsAColorMapSampledFromTheOpenedImage();
    testGenerateProducesADifferentResultOnTheSameImage();
    testOpeningAMissingFileLeavesControlsDisabled();

    return apo_test::reportAndExit();
}
