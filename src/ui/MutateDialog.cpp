#include "MutateDialog.h"

#include <algorithm>
#include <random>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "RenderWorker.h"
#include "SliderSpin.h"
#include "core/VariationRegistry.h"
#include "core/edit/MutationOps.h"

namespace apo::ui {

namespace {

// cell 0 is the center (row 1, col 1); cells 1..8 fill the remaining 8
// grid positions in reading order.
constexpr int kCellRow[MutateDialog::kGridCells] = {1, 0, 0, 0, 1, 1, 2, 2, 2};
constexpr int kCellCol[MutateDialog::kGridCells] = {1, 0, 1, 2, 0, 2, 0, 1, 2};

// XForm::symmetry doubles as two legacy Pascal-side concepts (a boolean
// "keep this transform fixed during mutation" flag and a continuous
// "color_speed" value - see FlameIO.cpp's parsing comment) - treating any
// nonzero value as "pinned" covers the flag's intent without needing to
// disambiguate which meaning a given .flame file's author intended.
void applyMaintainSymmetry(const apo::Flame& base, apo::Flame& mutant) {
    for (int i = 0; i < base.numXForms(); ++i) {
        if (base.xform[i]->symmetry != 0) {
            mutant.xform[i]->assign(*base.xform[i]);
        }
    }
}

} // namespace

MutateDialog::MutateDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(std::move(flame)) {
    setWindowTitle(
        QString("Mutate - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(720, 640);
    setAttribute(Qt::WA_DeleteOnClose);

    initialSeed_ = seed_ = static_cast<std::uint64_t>(std::random_device{}());

    auto* rootLayout = new QVBoxLayout(this);

    auto* controlsGroup = new QGroupBox("Directions", this);
    auto* form = new QFormLayout(controlsGroup);

    trendCombo_ = new QComboBox(controlsGroup);
    trendCombo_->setObjectName("trendCombo");
    trendCombo_->addItem("Random");
    const auto& registry = VariationRegistry::instance();
    for (int i = 0; i < registry.nrVar(); ++i) {
        trendCombo_->addItem(QString::fromStdString(registry.varName(i)));
    }
    form->addRow("Trend", trendCombo_);

    strengthCtrl_ = new SliderSpin("Strength", 0, 100, strength_ * 100.0, 0, controlsGroup);
    strengthCtrl_->setObjectName("strengthCtrl");
    form->addRow(strengthCtrl_);

    sameNumCheck_ = new QCheckBox("Keep same number of transforms", controlsGroup);
    sameNumCheck_->setObjectName("sameNumCheck");
    form->addRow(sameNumCheck_);

    maintainSymmetryCheck_ = new QCheckBox("Maintain symmetry", controlsGroup);
    maintainSymmetryCheck_->setObjectName("maintainSymmetryCheck");
    form->addRow(maintainSymmetryCheck_);

    rootLayout->addWidget(controlsGroup);

    auto* grid = new QGridLayout();
    grid->setSpacing(4);
    for (int cell = 0; cell < kGridCells; ++cell) {
        auto* button = new QPushButton(this);
        button->setObjectName(QString("cellButton%1").arg(cell));
        button->setFlat(cell != 0);
        button->setMinimumSize(100, 100);
        button->setToolTip(cell == 0 ? "Click to generate a new batch of mutants"
                                      : "Click to adopt this mutant as the current flame");
        connect(button, &QPushButton::clicked, this, [this, cell] { onCellClicked(cell); });
        grid->addWidget(button, kCellRow[cell], kCellCol[cell]);
        cellButtons_[cell] = button;
    }
    for (int i = 0; i < 3; ++i) {
        grid->setRowStretch(i, 1);
        grid->setColumnStretch(i, 1);
    }
    rootLayout->addLayout(grid, /*stretch=*/1);

    auto* bottomRow = new QHBoxLayout();
    backButton_ = new QPushButton("Back", this);
    backButton_->setObjectName("backButton");
    backButton_->setEnabled(false);
    connect(backButton_, &QPushButton::clicked, this, &MutateDialog::onBack);
    bottomRow->addWidget(backButton_);
    bottomRow->addStretch(1);
    rootLayout->addLayout(bottomRow);

    connect(trendCombo_, &QComboBox::currentIndexChanged, this, &MutateDialog::onTrendChanged);
    connect(strengthCtrl_, &SliderSpin::valueChanged, this, &MutateDialog::onStrengthChanged);
    connect(sameNumCheck_, &QCheckBox::toggled, this, &MutateDialog::onSameNumToggled);
    connect(maintainSymmetryCheck_, &QCheckBox::toggled, this, &MutateDialog::onMaintainSymmetryToggled);

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &MutateDialog::renderRequested, worker_, &RenderWorker::renderFlame);
    connect(worker_, &RenderWorker::renderFinished, this, &MutateDialog::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    regenerateMutants();
}

MutateDialog::~MutateDialog() {
    workerThread_->quit();
    workerThread_->wait();
}

void MutateDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void MutateDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    // The very first grid render is requested before the grid layout has
    // assigned the cell buttons their final size (same issue AdjustDialog's
    // preview label had) - re-requesting on every resize (safe: coalesced
    // by the pending-cell queue, same as everywhere else in this dialog)
    // keeps every cell's icon sized to its button rather than stuck at
    // whatever size it first rendered at.
    requestGridRefresh();
}

void MutateDialog::regenerateMutants() {
    apo::Xoshiro256Rng rng(seed_);
    const int lo = sameNumTransforms_ ? std::max(1, flame_->numXForms()) : AppSettings::mutationMinXforms();
    const int hi = sameNumTransforms_ ? std::max(1, flame_->numXForms()) : AppSettings::mutationMaxXforms();
    // Options.pas's Variations tab checklist restricts the "Random" trend
    // (trendVariationIndex_ < 0) the same way it restricts New Random
    // Batch - see AppSettings.h's disabledVariationNames() doc comment. A
    // forced trend variation bypasses this entirely (randomizeXforms's own
    // variationIndex>=0 short-circuit), so it's harmless to always pass.
    const std::vector<int> eligibleVariations = AppSettings::enabledVariationIndices();
    for (auto& mutant : mutants_) {
        mutant = std::make_unique<apo::Flame>();
        apo::randomizeXforms(*mutant, rng, lo, hi, trendVariationIndex_, &eligibleVariations);
    }
    requestGridRefresh();
}

std::unique_ptr<apo::Flame> MutateDialog::buildPreviewFlame(int cell) const {
    std::unique_ptr<apo::Flame> preview;
    if (cell == 0) {
        preview = flame_->clone();
    } else {
        preview = apo::blendXforms(*flame_, *mutants_[cell - 1], strength_);
        if (maintainSymmetry_) applyMaintainSymmetry(*flame_, *preview);
    }

    const QPushButton* button = cellButtons_[cell];
    const int w = std::max(button->width(), 64);
    const int h = std::max(button->height(), 64);
    preview->adjustScale(w, h);
    preview->sampleDensity = AppSettings::previewSampleDensity();
    return preview;
}

void MutateDialog::requestGridRefresh() {
    // Guards against a resizeEvent firing before regenerateMutants() has
    // populated mutants_ for the first time (e.g. the constructor's own
    // initial resize() call) - buildPreviewFlame() would otherwise
    // dereference a null mutant.
    if (!mutants_[0]) return;

    pendingHead_ = 0;
    pendingCount_ = kGridCells;
    for (int i = 0; i < kGridCells; ++i) pendingCells_[i] = i;
    renderNextCell();
}

void MutateDialog::renderNextCell() {
    if (renderInFlight_ || pendingCount_ == 0) return;

    currentCell_ = pendingCells_[pendingHead_];
    pendingHead_ = (pendingHead_ + 1) % kGridCells;
    --pendingCount_;
    renderInFlight_ = true;

    std::shared_ptr<const apo::Flame> shared(buildPreviewFlame(currentCell_));
    emit renderRequested(shared, /*seed=*/1);
}

void MutateDialog::onRenderFinished(QImage image, quint64 /*pointsGenerated*/, quint64 /*pointsAccepted*/) {
    if (!image.isNull() && currentCell_ >= 0) {
        QPushButton* button = cellButtons_[currentCell_];
        button->setIcon(QIcon(QPixmap::fromImage(image)));
        button->setIconSize(button->size());
    }

    renderInFlight_ = false;
    renderNextCell();

    if (pendingCount_ == 0 && !renderInFlight_ && !autoScreenshotPath_.isEmpty()) {
        const QString path = autoScreenshotPath_;
        const bool exitAfter = autoScreenshotExit_;
        autoScreenshotPath_.clear();
        QTimer::singleShot(100, this, [this, path, exitAfter] {
            grab().save(path, "PNG");
            if (exitAfter) qApp->quit();
        });
    }
}

void MutateDialog::onCellClicked(int cell) {
    if (cell == 0) {
        ++seed_;
        backButton_->setEnabled(seed_ > initialSeed_);
        regenerateMutants();
        return;
    }

    auto blended = apo::blendXforms(*flame_, *mutants_[cell - 1], strength_);
    if (maintainSymmetry_) applyMaintainSymmetry(*flame_, *blended);
    flame_->copyFrom(*blended);

    // The same mutants_[] batch is kept (no reroll) - matches
    // Mutate.pas's MutantClick, which re-interpolates the existing batch
    // around the newly adopted flame rather than generating a fresh one;
    // that's what lets repeated clicks "walk" toward a result.
    requestGridRefresh();
    emit flameChanged();
}

void MutateDialog::onBack() {
    if (seed_ > initialSeed_) --seed_;
    backButton_->setEnabled(seed_ > initialSeed_);
    regenerateMutants();
}

void MutateDialog::onTrendChanged(int index) {
    if (index < 0) return;
    trendVariationIndex_ = index - 1; // combo index 0 = "Random" (-1)

    apo::Xoshiro256Rng rng(seed_);
    const std::vector<int> eligibleVariations = AppSettings::enabledVariationIndices();
    for (auto& mutant : mutants_) {
        if (mutant) apo::setXformsVariation(*mutant, rng, trendVariationIndex_, &eligibleVariations);
    }
    requestGridRefresh();
}

void MutateDialog::onStrengthChanged(double value) {
    strength_ = value / 100.0;
    requestGridRefresh();
}

void MutateDialog::onSameNumToggled(bool checked) {
    sameNumTransforms_ = checked;
    regenerateMutants(); // matches chkSameNumClick calling RandomSet directly
}

void MutateDialog::onMaintainSymmetryToggled(bool checked) {
    maintainSymmetry_ = checked;
    requestGridRefresh(); // no reroll - matches mnuMaintainSymClick
}

} // namespace apo::ui
