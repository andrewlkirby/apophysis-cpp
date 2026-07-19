#pragma once

#include <array>
#include <memory>

#include <QDialog>
#include <QImage>
#include <QString>

#include "core/Flame.h"

class QThread;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QResizeEvent;

namespace apo::ui {

class RenderWorker;
class SliderSpin;

// Port of Mutate.pas (695 lines) - a 3x3 grid: the current flame in the
// center, 8 randomly-generated "mutants" around it, each a blend of the
// current flame and a fresh random one at the current Strength. Clicking a
// mutant adopts that blend as the new current flame (in place, so the
// owning EditorWindow's shared_ptr<Flame> stays valid - see
// Flame::copyFrom); clicking the center regenerates a fresh batch of
// mutants around the *same* current flame. Deliberately scoped down per
// the "good performance, not a faithful copy" project direction - see
// core/edit/MutationOps.h's header comment for the engine-level cuts
// (no auto-framing/CalcBoundbox, no colormap randomization, no
// independently-random-per-xform variation flavor). UI-level cuts:
//   - No Reset Location toggle - mutants and adopted flames always keep
//     the base flame's own camera framing (matches the original's own
//     default, unchecked, behavior - see MutationOps.h).
//   - No per-dialog preview-quality menu - reuses AppSettings::
//     previewSampleDensity() (Options dialog), the same knob AdjustDialog
//     uses, rather than a second near-duplicate setting.
//   - No registry-persisted window position.
// Kept: Trend (variation) combo, Strength slider, "keep same number of
// transforms" checkbox, Maintain Symmetry checkbox (a real, data-driven
// feature - see XForm::symmetry, read from real .flame files), Next
// (click center) / Back seed navigation.
class MutateDialog final : public QDialog {
    Q_OBJECT

public:
    // cell 0 is the center (the current flame); cells 1..8 are the 8
    // mutants, backed by mutants_[0..7]. Public so the cell-position/label
    // tables in MutateDialog.cpp's anonymous namespace can size against it.
    static constexpr int kGridCells = 9;
    static constexpr int kMutantCount = 8;

    explicit MutateDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);
    ~MutateDialog() override;

    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    // Emitted after adopting a mutant (flame_ is mutated in place) so an
    // owning window (EditorWindow) can refresh its own view of it.
    void flameChanged();
    void renderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed);

private slots:
    void onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted);
    void onCellClicked(int cell);
    void onBack();
    void onTrendChanged(int index);
    void onStrengthChanged(double value);
    void onSameNumToggled(bool checked);
    void onMaintainSymmetryToggled(bool checked);

private:
    void regenerateMutants();  // rerolls mutants_[] from a fresh Rng(seed_)
    void requestGridRefresh(); // re-blends + re-renders every cell, no reroll
    void renderNextCell();
    std::unique_ptr<apo::Flame> buildPreviewFlame(int cell) const;

    std::shared_ptr<apo::Flame> flame_;
    std::array<std::unique_ptr<apo::Flame>, kMutantCount> mutants_;

    std::uint64_t initialSeed_ = 0;
    std::uint64_t seed_ = 0;
    double strength_ = 0.35;
    int trendVariationIndex_ = -1; // -1 = "Random"
    bool sameNumTransforms_ = false;
    bool maintainSymmetry_ = false;

    QComboBox* trendCombo_ = nullptr;
    SliderSpin* strengthCtrl_ = nullptr;
    QCheckBox* sameNumCheck_ = nullptr;
    QCheckBox* maintainSymmetryCheck_ = nullptr;
    QPushButton* backButton_ = nullptr;
    std::array<QPushButton*, kGridCells> cellButtons_{};

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;
    std::array<int, kGridCells> pendingCells_{};
    int pendingCount_ = 0;
    int pendingHead_ = 0;
    int currentCell_ = -1;
    bool renderInFlight_ = false;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
