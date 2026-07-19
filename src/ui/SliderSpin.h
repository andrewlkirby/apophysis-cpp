#pragma once

#include <QWidget>

class QSlider;
class QDoubleSpinBox;

namespace apo::ui {

// A labeled row combining a QSlider (coarse, mouse-drag adjustment) and a
// QDoubleSpinBox (precise keyboard entry) that stay in sync - the modern
// equivalent of Adjust.pas's paired TScrollBar+TEdit rows. Used identically
// across AdjustDialog's Camera/Coloring/Gradient tabs (13 near-identical
// instances), which is what justifies pulling the sync logic into a shared
// widget rather than repeating it 13 times.
class SliderSpin final : public QWidget {
    Q_OBJECT

public:
    // `decimals` controls the spin box's displayed precision and the
    // slider's internal step resolution (the slider's integer range is
    // [min,max] scaled by 10^decimals internally, converted back to a
    // double at the public API boundary).
    SliderSpin(const QString& label, double min, double max, double value, int decimals, QWidget* parent = nullptr);

    double value() const;
    // Sets the value without emitting valueChanged/editingFinished - for
    // programmatic sync (e.g. refreshing controls after an undo), which
    // must not itself be treated as a new edit.
    void setValue(double v);
    // Reconfigures the slider/spin box range and resets to `value` -
    // AdjustDialog's Gradient tab reuses one SliderSpin across several
    // modes (Rotate/Hue/Saturation/...) that each have a different
    // amount range, rather than constructing a new widget per mode.
    void setRange(double min, double max, double value);

signals:
    // Fired continuously while dragging the slider or typing in the spin
    // box - live preview should hook this.
    void valueChanged(double v);
    // Fired once when a drag/edit gesture completes (slider released, spin
    // box editing finished via Enter or focus-out) - undo-snapshot commits
    // should hook this, not valueChanged, to avoid pushing one undo entry
    // per pixel of drag.
    void editingFinished(double v);

private:
    QSlider* slider_ = nullptr;
    QDoubleSpinBox* spin_ = nullptr;
    int scale_ = 1;
    bool syncing_ = false;
};

} // namespace apo::ui
