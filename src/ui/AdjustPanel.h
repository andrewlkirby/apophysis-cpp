#pragma once

#include <array>
#include <memory>

#include <QWidget>

#include "core/ColorMap.h"
#include "core/Flame.h"

class QEvent;
class QLabel;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;

namespace apo::ui {

class SliderSpin;

// The Camera/Coloring/Gradient/Size control tabs originally built inline by
// AdjustDialog, extracted into their own embeddable QWidget so EditorWindow
// can host them directly as a right-hand panel (see EditorWindow.h's own
// class comment on the Transform/Adjust panel switcher) instead of only
// through a separate top-level dialog. AdjustDialog itself now composes one
// of these next to its own preview label - see its header comment - so this
// is the single implementation of every Adjust control, not a fork.
//
// Deliberately owns no preview and no undo/redo of its own (unlike the
// original standalone-dialog design this was extracted from): whoever hosts
// this widget already has both a live preview (EditorWindow's canvas_,
// AdjustDialog's own previewLabel_) and a place to put undo entries, so
// duplicating either here would just be two competing sources of truth on
// the same shared Flame. Instead this follows exactly TransformPanel's own
// contract: propertyEdited() fires live on every change (the host re-renders
// its own preview), editingStarted()/editingFinished() bracket one commit-
// worthy gesture for the host's own undo stack to snapshot around.
class AdjustPanel final : public QWidget {
    Q_OBJECT

public:
    explicit AdjustPanel(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);

    // Re-reads every control from the current Flame state - for the host to
    // call after any edit that didn't originate from this panel (undo/redo,
    // a Transform-tab edit that happened to touch a shared field, switching
    // back to this panel after time away). If the flame's gradient was
    // changed outside this panel, the Gradient tab's per-mode amounts reset
    // and apply on top of that new gradient; otherwise they are kept.
    void refreshControlsFromFlame();

signals:
    // Fired continuously while a value is being edited (matches
    // TransformPanel::propertyEdited()'s contract exactly).
    void propertyEdited();
    void editingStarted();
    void editingFinished();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildCameraTab();
    QWidget* buildColoringTab();
    QWidget* buildGradientTab();
    QWidget* buildSizeTab();

    void refreshGradientStrip();
    void refreshBackgroundSwatch();
    void pickBackgroundColor();
    void setGradientMode(int modeIndex);
    void applyGradientAmount(double amount);
    void recomputeGradient();
    // Replaces the gradient the per-mode amounts are applied to and resets
    // every amount to its mode's default (used by Invert/Reverse/Randomize/
    // Reset/Browse and by external edits such as undo).
    void setGradientSource(const apo::ColorMap& source);
    void openGradientBrowser();

    void beginEditIfNeeded();
    void commitEditIfNeeded();

    std::shared_ptr<apo::Flame> flame_;
    apo::ColorMap initialCmap_;      // for the Gradient tab's Reset button
    // The Gradient tab's modes are non-destructive: flame_->cmap is always
    // recomputed as gradientSource_ with every mode's remembered amount
    // applied, so switching modes keeps each mode's value and returning a
    // slider to its default exactly restores the unadjusted gradient.
    apo::ColorMap gradientSource_;
    apo::ColorMap gradientOutput_; // last value written to flame_->cmap by recomputeGradient()
    std::array<double, 7> gradientAmounts_{};
    bool hasPendingGesture_ = false;

    // Camera tab
    SliderSpin* zoomCtrl_ = nullptr;
    SliderSpin* centerXCtrl_ = nullptr;
    SliderSpin* centerYCtrl_ = nullptr;
    SliderSpin* angleCtrl_ = nullptr;
    SliderSpin* pitchCtrl_ = nullptr;
    SliderSpin* yawCtrl_ = nullptr;
    SliderSpin* perspCtrl_ = nullptr;
    SliderSpin* dofCtrl_ = nullptr;
    SliderSpin* zposCtrl_ = nullptr;

    // Coloring tab
    SliderSpin* gammaCtrl_ = nullptr;
    SliderSpin* brightnessCtrl_ = nullptr;
    SliderSpin* vibrancyCtrl_ = nullptr;
    SliderSpin* gammaThresholdCtrl_ = nullptr;
    QPushButton* backgroundButton_ = nullptr;

    // Gradient tab
    QLabel* gradientStrip_ = nullptr;
    QComboBox* gradientModeCombo_ = nullptr;
    SliderSpin* gradientAmountCtrl_ = nullptr;

    // Size tab
    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QCheckBox* maintainAspectCheck_ = nullptr;
    double sizeAspectRatio_ = 1.0;
};

} // namespace apo::ui
