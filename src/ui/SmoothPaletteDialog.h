#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

#include "core/ColorMap.h"

class QLabel;
class QPushButton;
class QShowEvent;

namespace apo::ui {

// A5 - Main.pas's mnuSmoothGradientClick/SmoothPalette (File > "Smooth
// Palette..."), deriving a smooth 256-entry gradient from an arbitrary
// image via core/edit/SmoothPalette.h's ported algorithm. Same
// no-target-Flame shape as GradientBrowserDialog (see its own doc comment):
// this dialog never touches a Flame directly - Apply just emits the
// computed ColorMap, and the caller (MainWindow, for the selected library
// flame - see onSmoothPaletteTriggered()) decides what to do with it.
//
// Broadened beyond the original's own Browse filter (bmp/dib/jpg/jpeg
// only): any image format Qt's QImageReader supports (png/gif/bmp/jpg/...)
// - QImage already decodes all of those for free, so restricting the
// filter to the original's narrower list would be a purely artificial
// limitation for no benefit.
//
// Added beyond the original: a "Generate" button. The algorithm (see
// SmoothPalette.h's own doc comment) is stochastic - a fresh random seed on
// every run - so the original's own flow (pick an image, get exactly one
// result, done) leaves a user with no way to just try again if they don't
// like that particular sample/reorder. Generate re-runs the same already-
// loaded image with a new seed without needing to re-browse.
class SmoothPaletteDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SmoothPaletteDialog(QWidget* parent = nullptr);

    void setAutoScreenshot(const QString& path, bool exitAfter);

    // Loads `path` immediately and runs one Generate pass, as if the user
    // had just browsed to it - used by interaction tests to avoid needing
    // a real QFileDialog round-trip (same testing rationale as
    // GradientBrowserDialog::openFile).
    void openImageFile(const QString& path);

protected:
    void showEvent(QShowEvent* event) override;

signals:
    void paletteApplied(apo::ColorMap cmap);

private slots:
    void browseImage();
    void regenerate();
    void applyPalette();

private:
    void refreshPreview();

    // RGB888, kept around (not just the file path) so repeated Generate
    // clicks re-sample without re-decoding the file each time.
    QImage sourceImage_;
    apo::ColorMap currentPalette_{};
    bool hasPalette_ = false;

    QLabel* pathLabel_ = nullptr;
    QLabel* previewStrip_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    QPushButton* generateButton_ = nullptr;
    QPushButton* applyButton_ = nullptr;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
