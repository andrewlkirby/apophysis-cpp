#include "SmoothPaletteDialog.h"

#include <cstddef>
#include <cstring>
#include <random>
#include <vector>

#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "core/Rng.h"
#include "core/edit/SmoothPalette.h"

namespace apo::ui {

namespace {
QImage colorMapToImage(const apo::ColorMap& cmap) {
    QImage image(256, 1, QImage::Format_RGB888);
    for (int i = 0; i < 256; ++i) {
        image.setPixelColor(i, 0, QColor(cmap.entries[static_cast<size_t>(i)][0], cmap.entries[static_cast<size_t>(i)][1],
                                          cmap.entries[static_cast<size_t>(i)][2]));
    }
    return image;
}

// QImage rows are padded to a 4-byte boundary (bytesPerLine() can exceed
// width*3 for Format_RGB888) - smoothPaletteFromImage wants a tightly-
// packed width*height*3 buffer, so this strips that padding out row by row.
std::vector<std::uint8_t> packRgb(const QImage& image) {
    std::vector<std::uint8_t> packed(static_cast<size_t>(image.width()) * static_cast<size_t>(image.height()) * 3);
    for (int y = 0; y < image.height(); ++y) {
        const uchar* row = image.constScanLine(y);
        std::memcpy(packed.data() + static_cast<size_t>(y) * static_cast<size_t>(image.width()) * 3, row,
                    static_cast<size_t>(image.width()) * 3);
    }
    return packed;
}
} // namespace

SmoothPaletteDialog::SmoothPaletteDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Smooth Palette");
    resize(420, 220);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* rootLayout = new QVBoxLayout(this);

    auto* pathRow = new QHBoxLayout();
    pathLabel_ = new QLabel("No image chosen", this);
    pathLabel_->setObjectName("pathLabel");
    pathLabel_->setWordWrap(true);
    browseButton_ = new QPushButton("Browse Image...", this);
    browseButton_->setObjectName("browseButton");
    connect(browseButton_, &QPushButton::clicked, this, &SmoothPaletteDialog::browseImage);
    pathRow->addWidget(pathLabel_, /*stretch=*/1);
    pathRow->addWidget(browseButton_);
    rootLayout->addLayout(pathRow);

    previewStrip_ = new QLabel(this);
    previewStrip_->setObjectName("previewStrip");
    previewStrip_->setMinimumHeight(32);
    previewStrip_->setScaledContents(true);
    previewStrip_->setStyleSheet("background-color: #202020;");
    rootLayout->addWidget(previewStrip_);

    auto* buttonRow = new QHBoxLayout();
    generateButton_ = new QPushButton("Generate", this);
    generateButton_->setObjectName("generateButton");
    generateButton_->setEnabled(false);
    generateButton_->setToolTip("Re-samples the image with a new random seed - the algorithm is stochastic, so "
                                 "each Generate produces a different (still smoothed) result.");
    connect(generateButton_, &QPushButton::clicked, this, &SmoothPaletteDialog::regenerate);
    applyButton_ = new QPushButton("Apply", this);
    applyButton_->setObjectName("applyButton");
    applyButton_->setEnabled(false);
    connect(applyButton_, &QPushButton::clicked, this, &SmoothPaletteDialog::applyPalette);
    buttonRow->addStretch(1);
    buttonRow->addWidget(generateButton_);
    buttonRow->addWidget(applyButton_);
    rootLayout->addLayout(buttonRow);
}

void SmoothPaletteDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void SmoothPaletteDialog::browseImage() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Image", QString(), "Images (*.bmp *.dib *.jpg *.jpeg *.png *.gif);;All files (*.*)");
    if (!path.isEmpty()) openImageFile(path);
}

void SmoothPaletteDialog::openImageFile(const QString& path) {
    QImage loaded(path);
    if (loaded.isNull()) {
        pathLabel_->setText(path + " (failed to load)");
        sourceImage_ = QImage();
        hasPalette_ = false;
        generateButton_->setEnabled(false);
        applyButton_->setEnabled(false);
        previewStrip_->clear();
        return;
    }

    sourceImage_ = loaded.convertToFormat(QImage::Format_RGB888);
    pathLabel_->setText(path);
    generateButton_->setEnabled(true);
    regenerate();
}

void SmoothPaletteDialog::regenerate() {
    if (sourceImage_.isNull()) return;

    const auto packed = packRgb(sourceImage_);
    apo::Xoshiro256Rng rng(static_cast<std::uint64_t>(std::random_device{}()));
    const apo::SmoothPaletteParams params; // defaults match Regstry.pas's own NumTries/TryLength fallbacks
    hasPalette_ = apo::smoothPaletteFromImage(packed.data(), sourceImage_.width(), sourceImage_.height(), rng, params,
                                               currentPalette_);
    applyButton_->setEnabled(hasPalette_);
    refreshPreview();
}

void SmoothPaletteDialog::refreshPreview() {
    if (hasPalette_) {
        previewStrip_->setPixmap(QPixmap::fromImage(colorMapToImage(currentPalette_)));
    } else {
        previewStrip_->clear();
    }
}

void SmoothPaletteDialog::applyPalette() {
    if (!hasPalette_) return;
    emit paletteApplied(currentPalette_);
}

void SmoothPaletteDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (autoScreenshotFired_ || autoScreenshotPath_.isEmpty()) return;
    autoScreenshotFired_ = true;
    const QString path = autoScreenshotPath_;
    const bool exitAfter = autoScreenshotExit_;
    QTimer::singleShot(100, this, [this, path, exitAfter] {
        grab().save(path, "PNG");
        if (exitAfter) qApp->quit();
    });
}

} // namespace apo::ui
