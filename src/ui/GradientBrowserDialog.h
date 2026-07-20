#pragma once

#include <QDialog>
#include <QString>

#include "core/ColorMap.h"

class QEvent;
class QListWidget;
class QLabel;
class QPushButton;
class QShowEvent;

namespace apo::ui {

// Port of Browser.pas (621 lines, "TGradientBrowser") - a picker over a
// .ugr/.gradient/.map gradient library file: browse to a file, see every
// named gradient it contains, preview one, Apply it. Deliberately scoped
// down per the "good performance, not a faithful copy" project direction:
//   - No Delete/Rename of entries within the loaded file - the original's
//     DeleteEntry/RenameIFS rewrite the external file in place, which is
//     an unusual, fairly dangerous capability (silently mutating a user's
//     gradient library from inside a picker dialog) relative to its value;
//     this port only ever reads gradient files.
//   - No hover-tooltip palette preview (ListViewInfoTip/TooltipTimer) -
//     redundant with the dedicated preview strip this dialog already
//     always shows on selection.
//   - No registry-persisted window position/size.
//   - The original's public `Render`/`PreviewDensity`/`zoom`/`Center`/`cp`
//     fields are dead code in Browser.pas itself (grep confirms `Render`
//     is never actually pointed at a flame anywhere in that file) - this
//     dialog never renders a flame at all, only ever the flat 256x1
//     gradient-strip image DrawPalette produces, so none of that carries
//     over.
//
// Unlike the original (which reaches into MainForm/EditForm/AdjustForm/
// MutateForm globals directly to push an applied gradient into whichever
// happen to be open), this dialog doesn't hold a target Flame at all -
// Apply just emits the picked ColorMap via a signal, and the caller (e.g.
// AdjustDialog's Gradient tab) decides what to do with it, including
// participating in that caller's own undo history the same way its
// Invert/Reverse/Randomize/Reset buttons already do.
//
// Added beyond the original: a "Built-in Library" source (core/
// BuiltinGradients.h's 701 mechanically-ported flam3-style gradients) as
// an alternative to "Browse..." - the original had no equivalent (its
// cmapdata.pas table fed a different, unrelated random-palette feature,
// never Browser.pas itself), but it closes a real gap: this dialog is
// otherwise useless without an external .ugr file on hand. Built-in is the
// default source when the dialog opens with no file given.
class GradientBrowserDialog final : public QDialog {
    Q_OBJECT

public:
    explicit GradientBrowserDialog(QWidget* parent = nullptr);

    void setAutoScreenshot(const QString& path, bool exitAfter);

    // Loads `path` immediately, as if the user had just browsed to it -
    // used by both the --gradient-browser CLI flag and interaction tests
    // to avoid needing a real QFileDialog round-trip.
    void openFile(const QString& path);

    // Switches back to the built-in library, as if the user had just
    // clicked the "Built-in Library" button - same testing rationale as
    // openFile().
    void openBuiltinLibrary();

protected:
    void showEvent(QShowEvent* event) override;

    // Explicitly matches Browser.pas's own ListViewKeyPress(#13) handler
    // rather than relying on QAbstractItemView's built-in Key_Return ->
    // activated() behavior - that behavior turned out to depend on
    // internal item-view/style details that aren't consistent across Qt
    // versions (observed: fires reliably on Qt 6.8, silently doesn't on
    // Qt 6.9's offscreen platform). Filtering list_'s own KeyPress events
    // and calling applySelected() directly makes this deterministic
    // regardless of that.
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void gradientApplied(apo::ColorMap cmap);

private slots:
    void browseFile();
    void onSelectionChanged(int row);
    void applySelected();

private:
    enum class Source { Builtin, File };

    void refreshPreview();

    Source source_ = Source::Builtin;
    QString currentPath_;
    apo::ColorMap currentPreview_{};
    bool hasSelection_ = false;

    QLabel* pathLabel_ = nullptr;
    QListWidget* list_ = nullptr;
    QLabel* previewStrip_ = nullptr;
    QPushButton* builtinButton_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    QPushButton* applyButton_ = nullptr;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
