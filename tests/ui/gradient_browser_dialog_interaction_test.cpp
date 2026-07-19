// Real-widget interaction tests for GradientBrowserDialog (src/ui/
// GradientBrowserDialog.h) - the underlying file parsing (core/io/
// GradientIO.h) and the built-in gradient library (core/BuiltinGradients.h)
// are already covered independently in gradient_io_test.cpp and
// builtin_gradients_test.cpp; this file verifies the Qt wiring on top of
// both: the dialog defaults to the built-in library, opening a .ugr file
// lists every named block in file order, opening a .map file lists it as
// one entry, selecting an entry updates the preview strip to exactly what
// the underlying source would produce directly, Apply (button, Enter key)
// emits gradientApplied with that exact ColorMap, "Built-in Library"
// switches back from a loaded file, and a malformed/missing file leaves
// the dialog in a clean empty state rather than crashing. Mirrors
// adjust_dialog_interaction_test.cpp's approach.

#include <cstdio>
#include <fstream>

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "../TestHelpers.h"
#include "GradientBrowserDialog.h"
#include "core/BuiltinGradients.h"
#include "core/io/GradientIO.h"

using apo_test::check;

namespace {

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    out << content;
}

// Two named blocks, in a deliberately non-alphabetical order so a test
// asserting file-order (not sorted-order) listing is meaningful.
const char* kTestUgr =
    "Zeta {\n"
    "\tgradient:\n"
    "\t index=0 color=255\n"
    "\t index=255 color=16711680\n"
    "\topacity: 0\n"
    "}\n"
    "Alpha {\n"
    "\tgradient:\n"
    "\t index=0 color=65280\n"
    "\t index=255 color=16776960\n"
    "\topacity: 0\n"
    "}\n";

void testDefaultsToBuiltinLibrary() {
    auto* dialog = new apo::ui::GradientBrowserDialog();

    auto* list = dialog->findChild<QListWidget*>("gradientList");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    auto* previewStrip = dialog->findChild<QLabel*>("previewStrip");
    if (!check(list && applyButton && previewStrip, "list, applyButton, and previewStrip found")) {
        delete dialog;
        return;
    }

    check(list->count() == apo::builtinGradientCount(),
          "the dialog opens with the full built-in library listed, with no file needed");
    check(list->item(0)->text() == QString::fromUtf8(apo::builtinGradientName(0)),
          "the first entry's name matches BuiltinGradients exactly");
    check(list->currentRow() == 0, "the first built-in gradient is auto-selected");
    check(applyButton->isEnabled(), "Apply is enabled for the auto-selected built-in gradient");
    check(!previewStrip->pixmap().isNull(), "the preview strip shows a real pixmap for the built-in default");

    delete dialog;
}

void testBuiltinSelectionMatchesBuiltinGradientsExactly() {
    auto* dialog = new apo::ui::GradientBrowserDialog();
    auto* list = dialog->findChild<QListWidget*>("gradientList");
    auto* previewStrip = dialog->findChild<QLabel*>("previewStrip");
    if (!check(list && previewStrip, "list and previewStrip found")) {
        delete dialog;
        return;
    }

    list->setCurrentRow(42);
    const apo::ColorMap expected = apo::builtinGradient(42);

    const QImage shown = previewStrip->pixmap().toImage();
    bool allMatch = true;
    for (int i = 0; i < 256 && allMatch; ++i) {
        const QColor c = shown.pixelColor(i, 0);
        if (c.red() != expected.entries[i][0] || c.green() != expected.entries[i][1] ||
            c.blue() != expected.entries[i][2]) {
            allMatch = false;
        }
    }
    check(allMatch, "selecting built-in gradient 42 shows exactly what BuiltinGradients::builtinGradient(42) returns");

    delete dialog;
}

void testBuiltinLibraryButtonSwitchesBackFromAFile() {
    const std::string path = "gradient_browser_test_switch_back.ugr";
    writeFile(path, kTestUgr);

    auto* dialog = new apo::ui::GradientBrowserDialog();
    dialog->openFile(QString::fromStdString(path));
    auto* list = dialog->findChild<QListWidget*>("gradientList");
    auto* builtinButton = dialog->findChild<QPushButton*>("builtinButton");
    if (!check(list && builtinButton, "list and builtinButton found")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }
    check(list->count() == 2, "the file's 2 entries are listed after openFile()");

    QTest::mouseClick(builtinButton, Qt::LeftButton);
    check(list->count() == apo::builtinGradientCount(),
          "clicking 'Built-in Library' switches back to the full built-in list");

    delete dialog;
    std::remove(path.c_str());
}

void testOpenUgrFileListsBlocksInFileOrder() {
    const std::string path = "gradient_browser_test.ugr";
    writeFile(path, kTestUgr);

    auto* dialog = new apo::ui::GradientBrowserDialog();
    dialog->openFile(QString::fromStdString(path));

    auto* list = dialog->findChild<QListWidget*>("gradientList");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    auto* previewStrip = dialog->findChild<QLabel*>("previewStrip");
    if (!check(list && applyButton && previewStrip, "list, applyButton, and previewStrip found")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    check(list->count() == 2, "a .ugr file with 2 named blocks lists exactly 2 entries");
    check(list->item(0)->text() == "Zeta", "entries are listed in file order (Zeta first), not sorted");
    check(list->item(1)->text() == "Alpha", "entries are listed in file order (Alpha second)");
    check(list->currentRow() == 0, "the first entry is auto-selected on open");
    check(applyButton->isEnabled(), "Apply is enabled once a valid entry is selected");
    check(!previewStrip->pixmap().isNull(), "the preview strip shows a real pixmap after opening a valid file");

    delete dialog;
    std::remove(path.c_str());
}

void testOpenMapFileListsAsOneEntry() {
    const std::string path = "gradient_browser_test.map";
    std::string content;
    for (int i = 0; i < 256; ++i) content += std::to_string(i) + " " + std::to_string(255 - i) + " 0\n";
    writeFile(path, content);

    auto* dialog = new apo::ui::GradientBrowserDialog();
    dialog->openFile(QString::fromStdString(path));

    auto* list = dialog->findChild<QListWidget*>("gradientList");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(list && applyButton, "list and applyButton found")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    check(list->count() == 1, "a .map file (one anonymous gradient) lists exactly one entry");
    check(applyButton->isEnabled(), "Apply is enabled for a .map file's single entry");

    delete dialog;
    std::remove(path.c_str());
}

void testSelectionMatchesGradientIOExactly() {
    const std::string path = "gradient_browser_test2.ugr";
    writeFile(path, kTestUgr);

    auto* dialog = new apo::ui::GradientBrowserDialog();
    dialog->openFile(QString::fromStdString(path));
    auto* list = dialog->findChild<QListWidget*>("gradientList");
    auto* previewStrip = dialog->findChild<QLabel*>("previewStrip");
    if (!check(list && previewStrip, "list and previewStrip found")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    list->setCurrentRow(1); // "Alpha"

    apo::ColorMap expected{};
    check(apo::loadGradientFile(path, expected, "Alpha"), "GradientIO can load the 'Alpha' block directly for comparison");

    const QImage shown = previewStrip->pixmap().toImage();
    bool allMatch = true;
    for (int i = 0; i < 256 && allMatch; ++i) {
        const QColor c = shown.pixelColor(i, 0);
        if (c.red() != expected.entries[i][0] || c.green() != expected.entries[i][1] ||
            c.blue() != expected.entries[i][2]) {
            allMatch = false;
        }
    }
    check(allMatch, "selecting 'Alpha' shows exactly the ColorMap GradientIO::loadGradientFile parses for that block");

    delete dialog;
    std::remove(path.c_str());
}

void testApplyButtonEmitsExactColorMap() {
    const std::string path = "gradient_browser_test3.ugr";
    writeFile(path, kTestUgr);

    auto* dialog = new apo::ui::GradientBrowserDialog();
    dialog->openFile(QString::fromStdString(path)); // "Zeta" auto-selected
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(applyButton != nullptr, "applyButton found")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    apo::ColorMap expected{};
    check(apo::loadGradientFile(path, expected, "Zeta"), "GradientIO can load the 'Zeta' block directly for comparison");

    apo::ColorMap received{};
    bool receivedSignal = false;
    QObject::connect(dialog, &apo::ui::GradientBrowserDialog::gradientApplied, dialog,
                      [&](apo::ColorMap cmap) {
                          received = cmap;
                          receivedSignal = true;
                      });

    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(receivedSignal, "clicking Apply emits gradientApplied");
    check(received.entries == expected.entries, "the emitted ColorMap exactly matches what GradientIO parses for the selected entry");

    delete dialog;
    std::remove(path.c_str());
}

void testEnterKeyAppliesLikeTheOriginalsListViewKeyPress() {
    const std::string path = "gradient_browser_test4.ugr";
    writeFile(path, kTestUgr);

    auto* dialog = new apo::ui::GradientBrowserDialog();
    dialog->openFile(QString::fromStdString(path));
    auto* list = dialog->findChild<QListWidget*>("gradientList");
    if (!check(list != nullptr, "list found")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    // Deliberately not calling dialog->show() here: on the offscreen QPA
    // platform, showing a QDialog and then immediately dispatching a
    // synthetic QTest::keyClick to one of its children hangs indefinitely
    // (reproduced directly - the process sits idle, waiting on some
    // window-exposed/activation condition offscreen never satisfies)
    // instead of just misdirecting the event the way the deprecated-
    // setActiveWindow() case elsewhere in this test suite does. setFocus()
    // alone is sufficient for QTest::keyClick to reach the right widget.
    list->setFocus();
    QSignalSpy spy(dialog, &apo::ui::GradientBrowserDialog::gradientApplied);
    QTest::keyClick(list, Qt::Key_Return);

    check(spy.count() == 1, "pressing Enter in the list applies the selected gradient, matching Browser.pas's "
                            "ListViewKeyPress(#13)");

    delete dialog;
    std::remove(path.c_str());
}

void testMissingFileLeavesCleanEmptyState() {
    auto* dialog = new apo::ui::GradientBrowserDialog();
    dialog->openFile("this_file_does_not_exist.ugr");

    auto* list = dialog->findChild<QListWidget*>("gradientList");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(list && applyButton, "list and applyButton found")) {
        delete dialog;
        return;
    }

    check(list->count() == 0, "a missing file leaves the list empty rather than crashing");
    check(!applyButton->isEnabled(), "Apply stays disabled with nothing loaded");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testDefaultsToBuiltinLibrary();
    testBuiltinSelectionMatchesBuiltinGradientsExactly();
    testBuiltinLibraryButtonSwitchesBackFromAFile();
    testOpenUgrFileListsBlocksInFileOrder();
    testOpenMapFileListsAsOneEntry();
    testSelectionMatchesGradientIOExactly();
    testApplyButtonEmitsExactColorMap();
    testEnterKeyAppliesLikeTheOriginalsListViewKeyPress();
    testMissingFileLeavesCleanEmptyState();

    return apo_test::reportAndExit();
}
