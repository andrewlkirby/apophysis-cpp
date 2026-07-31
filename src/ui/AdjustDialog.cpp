#include "AdjustDialog.h"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QKeySequence>
#include <QLabel>
#include <QResizeEvent>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include "AdjustPanel.h"
#include "AppSettings.h"
#include "PreviewSizing.h"
#include "RenderWorker.h"
#include "WindowGeometry.h"

namespace apo::ui {

bool AdjustDialog::FlameSnapshot::operator==(const FlameSnapshot& other) const {
    return zoom == other.zoom && centerX == other.centerX && centerY == other.centerY && angle == other.angle &&
           pitch == other.pitch && yaw == other.yaw && persp == other.persp && dof == other.dof &&
           zpos == other.zpos && gamma == other.gamma && brightness == other.brightness &&
           vibrancy == other.vibrancy && gammaThreshold == other.gammaThreshold && background == other.background &&
           cmap.entries == other.cmap.entries && width == other.width && height == other.height &&
           pixelsPerUnit == other.pixelsPerUnit;
}

AdjustDialog::AdjustDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(std::move(flame)) {
    setWindowTitle(QString("Adjust - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(900, 600);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* rootLayout = new QVBoxLayout(this);

    auto* toolbar = new QToolBar(this);
    undoAction_ = toolbar->addAction("Undo");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, &AdjustDialog::onUndo);
    redoAction_ = toolbar->addAction("Redo");
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, &AdjustDialog::onRedo);
    rootLayout->addWidget(toolbar);
    updateUndoRedoActions();

    auto* splitter = new QSplitter(this);

    previewLabel_ = new QLabel(splitter);
    previewLabel_->setMinimumSize(200, 150);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet("background-color: #202020;");
    // Deliberately not setScaledContents(true): onRenderFinished() already
    // scales the rendered pixmap to fit previewLabel_ with
    // Qt::KeepAspectRatio (matching MainWindow's own previewLabel_ display),
    // so scaledContents' unconditional stretch-to-fill would just squish it
    // back out of proportion. The very first preview render is requested
    // before the splitter has assigned this label its final layout size, so
    // that render comes back smaller than the label's eventual displayed
    // size until resizeEvent() below requests a freshly-sized one once real
    // geometry is known (and on every subsequent resize) - a brief
    // undersized-but-correctly-proportioned preview, not a stretched one.
    splitter->addWidget(previewLabel_);

    panel_ = new AdjustPanel(flame_, splitter);
    connect(panel_, &AdjustPanel::propertyEdited, this, &AdjustDialog::requestPreviewRender);
    connect(panel_, &AdjustPanel::editingStarted, this, &AdjustDialog::beginEdit);
    connect(panel_, &AdjustPanel::editingFinished, this, &AdjustDialog::commitEdit);
    splitter->addWidget(panel_);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    rootLayout->addWidget(splitter, /*stretch=*/1);

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &AdjustDialog::renderRequested, worker_, &RenderWorker::renderFlame);
    connect(worker_, &RenderWorker::renderFinished, this, &AdjustDialog::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    // After the full UI (in particular previewLabel_) is built, not right
    // after the resize() above: restoreGeometry() applied before the
    // layout's true minimum size is known can get silently grown back past
    // a smaller restored size once the rest of the constructor populates
    // the layout (see WindowGeometry.h's restoreWindowGeometry() doc
    // comment) - and requestPreviewRender() below should see the actually-
    // restored previewLabel_ size, not whatever it'd be at the default size.
    restoreWindowGeometry(this, "AdjustDialog");
    requestPreviewRender();
}

AdjustDialog::~AdjustDialog() {
    workerThread_->quit();
    workerThread_->wait();
}

void AdjustDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    requestPreviewRender();
}

void AdjustDialog::closeEvent(QCloseEvent* event) {
    saveWindowGeometry(this, "AdjustDialog");
    QDialog::closeEvent(event);
}

AdjustDialog::FlameSnapshot AdjustDialog::snapshot() const {
    FlameSnapshot s;
    s.zoom = flame_->zoom;
    s.centerX = flame_->center[0];
    s.centerY = flame_->center[1];
    s.angle = flame_->angle;
    s.pitch = flame_->cameraPitch;
    s.yaw = flame_->cameraYaw;
    s.persp = flame_->cameraPersp;
    s.dof = flame_->cameraDOF;
    s.zpos = flame_->cameraZpos;
    s.gamma = flame_->gamma;
    s.brightness = flame_->brightness;
    s.vibrancy = flame_->vibrancy;
    s.gammaThreshold = flame_->gammaThreshold;
    s.background = flame_->background;
    s.cmap = flame_->cmap;
    s.width = flame_->width;
    s.height = flame_->height;
    s.pixelsPerUnit = flame_->pixelsPerUnit;
    return s;
}

void AdjustDialog::applySnapshot(const FlameSnapshot& s) {
    flame_->zoom = s.zoom;
    flame_->center[0] = s.centerX;
    flame_->center[1] = s.centerY;
    flame_->angle = s.angle;
    flame_->cameraPitch = s.pitch;
    flame_->cameraYaw = s.yaw;
    flame_->cameraPersp = s.persp;
    flame_->cameraDOF = s.dof;
    flame_->cameraZpos = s.zpos;
    flame_->gamma = s.gamma;
    flame_->brightness = s.brightness;
    flame_->vibrancy = s.vibrancy;
    flame_->gammaThreshold = s.gammaThreshold;
    flame_->background = s.background;
    flame_->cmap = s.cmap;
    flame_->width = s.width;
    flame_->height = s.height;
    flame_->pixelsPerUnit = s.pixelsPerUnit;
}

void AdjustDialog::beginEdit() {
    if (hasPendingSnapshot_) return;
    pendingBefore_ = snapshot();
    hasPendingSnapshot_ = true;
}

void AdjustDialog::commitEdit() {
    if (!hasPendingSnapshot_) return;
    hasPendingSnapshot_ = false;
    const FlameSnapshot after = snapshot();
    if (pendingBefore_ == after) return;

    undoStack_.push_back({pendingBefore_, after});
    redoStack_.clear();
    updateUndoRedoActions();
    emit flameChanged();
}

void AdjustDialog::updateUndoRedoActions() {
    undoAction_->setEnabled(!undoStack_.empty());
    redoAction_->setEnabled(!redoStack_.empty());
}

void AdjustDialog::onUndo() {
    if (undoStack_.empty()) return;
    const UndoEntry entry = undoStack_.back();
    undoStack_.pop_back();
    applySnapshot(entry.before);
    redoStack_.push_back(entry);
    panel_->refreshControlsFromFlame();
    updateUndoRedoActions();
    requestPreviewRender();
    emit flameChanged();
}

void AdjustDialog::onRedo() {
    if (redoStack_.empty()) return;
    const UndoEntry entry = redoStack_.back();
    redoStack_.pop_back();
    applySnapshot(entry.after);
    undoStack_.push_back(entry);
    panel_->refreshControlsFromFlame();
    updateUndoRedoActions();
    requestPreviewRender();
    emit flameChanged();
}

void AdjustDialog::requestPreviewRender() {
    if (renderInFlight_) {
        renderDirty_ = true;
        return;
    }
    renderInFlight_ = true;

    auto previewFlame = flame_->clone();
    int pw, ph;
    fitPreviewSize(previewLabel_->width(), previewLabel_->height(), flame_->width, flame_->height, pw, ph);
    previewFlame->adjustScale(pw, ph);
    // Read fresh on every preview render (not cached) so a Preview Quality
    // change in OptionsDialog takes effect immediately on the very next
    // render, without needing this dialog to be reopened.
    previewFlame->sampleDensity = AppSettings::previewSampleDensity();

    std::shared_ptr<const apo::Flame> shared(std::move(previewFlame));
    emit renderRequested(shared, /*seed=*/1);
}

void AdjustDialog::onRenderFinished(QImage image, quint64 /*pointsGenerated*/, quint64 /*pointsAccepted*/) {
    if (!image.isNull()) {
        previewLabel_->setPixmap(
            QPixmap::fromImage(image).scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    renderInFlight_ = false;
    if (renderDirty_) {
        renderDirty_ = false;
        requestPreviewRender();
        return;
    }

    if (!autoScreenshotPath_.isEmpty()) {
        const QString path = autoScreenshotPath_;
        const bool exitAfter = autoScreenshotExit_;
        autoScreenshotPath_.clear();
        QTimer::singleShot(100, this, [this, path, exitAfter] {
            grab().save(path, "PNG");
            if (exitAfter) qApp->quit();
        });
    }
}

void AdjustDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

} // namespace apo::ui
