#include "EditorWindow.h"

#include <algorithm>
#include <random>

#include <QActionGroup>
#include <QCoreApplication>
#include <QFileDialog>
#include <QKeySequence>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QResizeEvent>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QToolBar>

#include "AdjustDialog.h"
#include "AppSettings.h"
#include "CurvesDialog.h"
#include "ForceSymmetryDialog.h"
#include "FullscreenView.h"
#include "MutateDialog.h"
#include "PostProcessDialog.h"
#include "RenderDialog.h"
#include "RenderWorker.h"
#include "TransformPanel.h"
#include "TriangleCanvas.h"
#include "XaosDialog.h"
#include "core/Rng.h"
#include "core/edit/FlameOps.h"
#include "core/edit/Symmetry.h"
#include "core/io/FlameIO.h"

namespace apo::ui {

namespace {
// Process-wide, not per-EditorWindow - so Copy in one open Editor and
// Paste in another (MainWindow can have several open at once) works, the
// same way a real clipboard would. Deliberately not the OS clipboard/a
// text interchange format (the original's own): every field XForm::assign
// copies (affine coefs, every variation weight, registered-variation
// parameters, colors, name...) has no existing text encoding in this port
// outside of a full .flame xform element, and round-tripping through that
// just to move one transform within the same process would be pure
// overhead for zero benefit.
std::unique_ptr<apo::XForm> g_transformClipboard;
} // namespace

EditorWindow::EditorWindow(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QMainWindow(parent), flame_(std::move(flame)) {
    setWindowTitle(QString("Edit - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(1000, 700);
    setAttribute(Qt::WA_DeleteOnClose);

    canvas_ = new TriangleCanvas(this);
    canvas_->setFlame(flame_);
    connect(canvas_, &TriangleCanvas::xformEdited, this, &EditorWindow::onXformEdited);
    connect(canvas_, &TriangleCanvas::editingStarted, this, &EditorWindow::onEditingStarted);
    connect(canvas_, &TriangleCanvas::editingFinished, this, &EditorWindow::onEditingFinished);
    connect(canvas_, &TriangleCanvas::selectedXformChanged, this, &EditorWindow::onCanvasSelectionChanged);

    xformList_ = new QListWidget(this);
    xformList_->setMaximumWidth(150);
    connect(xformList_, &QListWidget::currentRowChanged, this, &EditorWindow::onXformListSelectionChanged);

    transformPanel_ = new TransformPanel(this);
    transformPanel_->setFlame(flame_);
    transformPanel_->setMinimumWidth(280);
    connect(transformPanel_, &TransformPanel::propertyEdited, this, &EditorWindow::requestRender);
    connect(transformPanel_, &TransformPanel::editingStarted, this, &EditorWindow::onXformPropertyEditingStarted);
    connect(transformPanel_, &TransformPanel::editingFinished, this, &EditorWindow::onXformPropertyEditingFinished);

    refreshXformList();

    QSplitter* splitter = new QSplitter(this);
    splitter->addWidget(xformList_);
    splitter->addWidget(canvas_);
    splitter->addWidget(transformPanel_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    setCentralWidget(splitter);

    QAction* saveFlameAction = new QAction("Save &Flame As...", this);
    saveFlameAction->setObjectName("saveFlameAsAction");
    saveFlameAction->setShortcut(QKeySequence::Save);
    connect(saveFlameAction, &QAction::triggered, this, &EditorWindow::onSaveFlameAsTriggered);
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(saveFlameAction);

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    undoAction_ = editMenu->addAction("&Undo");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, &EditorWindow::onUndo);
    redoAction_ = editMenu->addAction("&Redo");
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, &EditorWindow::onRedo);
    updateUndoRedoActions();

    // A6 - matches Main.dfm's Flame menu weight/color utility commands -
    // see core/edit/FlameOps.h's own doc comment.
    QMenu* flameMenu = menuBar()->addMenu("F&lame");
    QAction* randomizeWeightsAction = flameMenu->addAction("&Random Weights");
    randomizeWeightsAction->setObjectName("randomizeWeightsAction");
    connect(randomizeWeightsAction, &QAction::triggered, this, &EditorWindow::onRandomizeWeights);
    QAction* equalizeWeightsAction = flameMenu->addAction("&Equalize Weights");
    equalizeWeightsAction->setObjectName("equalizeWeightsAction");
    connect(equalizeWeightsAction, &QAction::triggered, this, &EditorWindow::onEqualizeWeights);
    flameMenu->addSeparator();
    QAction* calculateColorsAction = flameMenu->addAction("&Calculate Colors Values");
    calculateColorsAction->setObjectName("calculateColorsAction");
    connect(calculateColorsAction, &QAction::triggered, this, &EditorWindow::onCalculateColorValues);
    QAction* randomizeColorsAction = flameMenu->addAction("Rando&mize Color Values");
    randomizeColorsAction->setObjectName("randomizeColorsAction");
    connect(randomizeColorsAction, &QAction::triggered, this, &EditorWindow::onRandomizeColorValues);

    QToolBar* toolbar = addToolBar("Edit");
    toolbar->setMovable(false);
    toolbar->addAction(undoAction_);
    toolbar->addAction(redoAction_);
    toolbar->addSeparator();

    auto* modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    QAction* moveAction = toolbar->addAction("Move");
    moveAction->setCheckable(true);
    moveAction->setChecked(true);
    modeGroup->addAction(moveAction);
    connect(moveAction, &QAction::triggered, this, [this] { canvas_->setEditMode(TriangleCanvas::EditMode::Move); });

    QAction* rotateAction = toolbar->addAction("Rotate");
    rotateAction->setCheckable(true);
    modeGroup->addAction(rotateAction);
    connect(rotateAction, &QAction::triggered, this,
            [this] { canvas_->setEditMode(TriangleCanvas::EditMode::Rotate); });

    QAction* scaleAction = toolbar->addAction("Scale");
    scaleAction->setCheckable(true);
    modeGroup->addAction(scaleAction);
    connect(scaleAction, &QAction::triggered, this, [this] { canvas_->setEditMode(TriangleCanvas::EditMode::Scale); });

    toolbar->addSeparator();
    QAction* postAction = toolbar->addAction("Post");
    postAction->setCheckable(true);
    postAction->setToolTip("Edit the post-transform instead of the main transform");
    connect(postAction, &QAction::toggled, this, [this](bool checked) {
        canvas_->setEditingPostTransform(checked);
        transformPanel_->setEditingPostTransform(checked);
    });
    // The Transform tab has its own "Edit post transform" checkbox (for
    // when the panel is used without reaching for the toolbar) - keep it
    // and this toolbar toggle mirroring each other in both directions.
    connect(transformPanel_, &TransformPanel::editingPostTransformChanged, postAction, &QAction::setChecked);

    toolbar->addSeparator();
    pickPivotAction_ = toolbar->addAction("Set Pivot");
    pickPivotAction_->setToolTip("Click a point on the canvas to rotate/scale around it instead of the "
                                  "triangle's center");
    connect(pickPivotAction_, &QAction::triggered, this, [this] {
        canvas_->setPivotPickArmed(true);
        statusBar()->showMessage("Click anywhere on the canvas to set the rotate/scale pivot...");
    });

    clearPivotAction_ = toolbar->addAction("Clear Pivot");
    connect(clearPivotAction_, &QAction::triggered, this, [this] {
        canvas_->clearCustomPivot();
        statusBar()->showMessage("Pivot reset to the selected triangle's center");
    });

    toolbar->addSeparator();
    QAction* fitAction = toolbar->addAction("Fit View");
    connect(fitAction, &QAction::triggered, canvas_, &TriangleCanvas::fitView);

    toolbar->addSeparator();
    addXformAction_ = toolbar->addAction("Add");
    addXformAction_->setObjectName("addXformAction");
    addXformAction_->setToolTip("Add a new transform");
    connect(addXformAction_, &QAction::triggered, this, &EditorWindow::onAddXform);
    duplicateXformAction_ = toolbar->addAction("Duplicate");
    duplicateXformAction_->setObjectName("duplicateXformAction");
    duplicateXformAction_->setToolTip("Duplicate the selected transform");
    // Matches Editor.pas's TriangleViewKeyDown: VK_INSERT triggers
    // Duplicate, not Add - a real behavior of the original, faithfully
    // kept since it costs nothing and a user coming from the original
    // would expect it.
    duplicateXformAction_->setShortcut(QKeySequence(Qt::Key_Insert));
    connect(duplicateXformAction_, &QAction::triggered, this, &EditorWindow::onDuplicateXform);
    deleteXformAction_ = toolbar->addAction("Delete");
    deleteXformAction_->setObjectName("deleteXformAction");
    deleteXformAction_->setToolTip("Delete the selected transform");
    deleteXformAction_->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(deleteXformAction_, &QAction::triggered, this, &EditorWindow::onDeleteXform);

    finalXformAction_ = toolbar->addAction("Final Transform");
    finalXformAction_->setObjectName("finalXformAction");
    finalXformAction_->setCheckable(true);
    finalXformAction_->setToolTip("A transform applied once, after every iteration - unlike a regular "
                                   "transform, it's never picked probabilistically");
    connect(finalXformAction_, &QAction::toggled, this, &EditorWindow::onFinalXformToggled);

    QAction* copyXformAction = toolbar->addAction("Copy");
    copyXformAction->setObjectName("copyXformAction");
    copyXformAction->setToolTip("Copy the selected transform (coefficients, variations, colors, name)");
    connect(copyXformAction, &QAction::triggered, this, &EditorWindow::onCopyXform);
    QAction* pasteXformAction = toolbar->addAction("Paste");
    pasteXformAction->setObjectName("pasteXformAction");
    pasteXformAction->setToolTip("Paste the last-copied transform onto the selected one");
    connect(pasteXformAction, &QAction::triggered, this, &EditorWindow::onPasteXform);

    QAction* xaosAction = toolbar->addAction("Xaos...");
    xaosAction->setObjectName("xaosAction");
    connect(xaosAction, &QAction::triggered, this, &EditorWindow::openXaosDialog);
    QAction* forceSymmetryAction = toolbar->addAction("Force Symmetry...");
    forceSymmetryAction->setObjectName("forceSymmetryAction");
    connect(forceSymmetryAction, &QAction::triggered, this, &EditorWindow::openForceSymmetryDialog);

    toolbar->addSeparator();
    QAction* adjustAction = toolbar->addAction("Adjust...");
    connect(adjustAction, &QAction::triggered, this, &EditorWindow::openAdjustDialog);
    QAction* renderAction = toolbar->addAction("Render...");
    connect(renderAction, &QAction::triggered, this, &EditorWindow::openRenderDialog);
    QAction* mutateAction = toolbar->addAction("Mutate...");
    connect(mutateAction, &QAction::triggered, this, &EditorWindow::openMutateDialog);
    QAction* curvesAction = toolbar->addAction("Curves...");
    connect(curvesAction, &QAction::triggered, this, &EditorWindow::openCurvesDialog);
    QAction* postProcessAction = toolbar->addAction("Post Process...");
    connect(postProcessAction, &QAction::triggered, this, &EditorWindow::openPostProcessDialog);
    QAction* fullscreenAction = toolbar->addAction("Fullscreen");
    connect(fullscreenAction, &QAction::triggered, this, &EditorWindow::openFullscreenView);

    statusBar()->showMessage("Ready");

    // Same dedicated-worker-thread pattern as MainWindow (see
    // RenderWorker.h) - but here, driven by triangle drags rather than a
    // one-shot file open, so requestRender() coalesces bursts of
    // xformEdited signals (one per mouse-move) into "at most one render in
    // flight, always eventually reflecting the latest state" rather than
    // queuing every intermediate frame.
    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &EditorWindow::renderRequested, worker_, &RenderWorker::renderFlame);
    connect(worker_, &RenderWorker::renderFinished, this, &EditorWindow::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    requestRender();
}

EditorWindow::~EditorWindow() {
    workerThread_->quit();
    workerThread_->wait();
}

void EditorWindow::refreshXformList() {
    xformList_->blockSignals(true);
    xformList_->clear();
    for (int i = 0; i < flame_->numXForms(); ++i) {
        xformList_->addItem(QString("Xform %1").arg(i + 1));
    }
    if (flame_->finalXformEnabled) xformList_->addItem("Final");

    const int sel = canvas_->selectedXform();
    const int row = listRowForXformIndex(sel);
    if (row >= 0 && row < xformList_->count()) xformList_->setCurrentRow(row);
    xformList_->blockSignals(false);

    // Undo/redo can restore a flame_ whose finalXformEnabled disagrees
    // with the toolbar action's current checked state (copyFrom()
    // overwrites flame_ wholesale but has no way to reach into this
    // window's own widgets) - this is the one central place (like the
    // transformPanel_->setSelectedXform() call below) every structural
    // change already routes through, so it's also the one place that
    // needs to resync it. blockSignals so this doesn't loop back into
    // onFinalXformToggled.
    // Guarded: the constructor's own first refreshXformList() call (see
    // below) runs before the toolbar - and finalXformAction_ - exist yet.
    if (finalXformAction_) {
        finalXformAction_->blockSignals(true);
        finalXformAction_->setChecked(flame_->finalXformEnabled);
        finalXformAction_->blockSignals(false);
    }

    // TriangleCanvas::setSelectedXform() (which every programmatic
    // selection change in this file calls) never emits
    // selectedXformChanged - only a real canvas click does (see
    // TriangleCanvas.cpp) - so refreshXformList() re-syncing from
    // canvas_->selectedXform() is also transformPanel_'s one central sync
    // point for every non-click selection change (Add/Duplicate/Delete/
    // Undo/Redo/Force Symmetry all call this already).
    transformPanel_->setSelectedXform(sel);
}

int EditorWindow::xformIndexForListRow(int row) const {
    if (row < 0) return -1;
    if (flame_->finalXformEnabled && row == flame_->numXForms()) return apo::kFinalXformIndex;
    return row;
}

int EditorWindow::listRowForXformIndex(int index) const {
    if (index == apo::kFinalXformIndex) return flame_->finalXformEnabled ? flame_->numXForms() : -1;
    return index;
}

apo::XForm& EditorWindow::xformAt(int index) {
    return index == apo::kFinalXformIndex ? *flame_->finalXform : *flame_->xform[index];
}

void EditorWindow::onXformEdited(int /*index*/) {
    // A canvas drag edits XForm::c/p directly, bypassing TransformPanel -
    // its Transform tab needs an explicit resync (every other kind of edit
    // in this window originates *from* TransformPanel, so it's always
    // already current).
    transformPanel_->refreshTransformTabFromCanvas();
    requestRender();
}

void EditorWindow::onEditingStarted(int index) {
    const apo::XForm& xf = xformAt(index);
    pendingEntry_.xformIndex = index;
    pendingEntry_.beforeC = xf.c;
    pendingEntry_.beforeP = xf.p;
    hasPendingEntry_ = true;
}

void EditorWindow::onEditingFinished(int index) {
    if (!hasPendingEntry_ || pendingEntry_.xformIndex != index) return;
    hasPendingEntry_ = false;

    const apo::XForm& xf = xformAt(index);
    pendingEntry_.afterC = xf.c;
    pendingEntry_.afterP = xf.p;

    // A click that didn't actually move anything (e.g. selecting a vertex
    // then releasing without moving the mouse) shouldn't clutter the undo
    // stack with a no-op entry.
    if (pendingEntry_.afterC == pendingEntry_.beforeC && pendingEntry_.afterP == pendingEntry_.beforeP) return;

    undoStack_.push_back(pendingEntry_);
    redoStack_.clear();
    updateUndoRedoActions();
}

void EditorWindow::onXformPropertyEditingStarted() {
    if (hasPendingPropertyEdit_) return;
    hasPendingPropertyEdit_ = true;
    pendingPropertyBefore_ = flame_->clone();
}

void EditorWindow::onXformPropertyEditingFinished() {
    if (!hasPendingPropertyEdit_) return;
    hasPendingPropertyEdit_ = false;
    const int sel = canvas_->selectedXform();
    pushStructuralUndo(*pendingPropertyBefore_, sel, sel);
    pendingPropertyBefore_.reset();
}

void EditorWindow::onUndo() {
    if (undoStack_.empty()) return;
    const UndoEntry entry = undoStack_.back();
    undoStack_.pop_back();

    if (entry.kind == UndoKind::Drag) {
        apo::XForm& xf = xformAt(entry.xformIndex);
        xf.c = entry.beforeC;
        xf.p = entry.beforeP;
        canvas_->setSelectedXform(entry.xformIndex);
    } else {
        flame_->copyFrom(*entry.beforeFlame);
        canvas_->setSelectedXform(entry.selectAfterUndo);
        refreshXformList();
        refreshXaosDialogIfOpen();
    }

    redoStack_.push_back(entry);
    canvas_->update();
    updateUndoRedoActions();
    requestRender();
}

void EditorWindow::onRedo() {
    if (redoStack_.empty()) return;
    const UndoEntry entry = redoStack_.back();
    redoStack_.pop_back();

    if (entry.kind == UndoKind::Drag) {
        apo::XForm& xf = xformAt(entry.xformIndex);
        xf.c = entry.afterC;
        xf.p = entry.afterP;
        canvas_->setSelectedXform(entry.xformIndex);
    } else {
        flame_->copyFrom(*entry.afterFlame);
        canvas_->setSelectedXform(entry.selectAfterRedo);
        refreshXformList();
        refreshXaosDialogIfOpen();
    }

    undoStack_.push_back(entry);
    canvas_->update();
    updateUndoRedoActions();
    requestRender();
}

void EditorWindow::updateUndoRedoActions() {
    undoAction_->setEnabled(!undoStack_.empty());
    redoAction_->setEnabled(!redoStack_.empty());
}

void EditorWindow::pushStructuralUndo(const apo::Flame& before, int selectAfterUndo, int selectAfterRedo) {
    UndoEntry entry;
    entry.kind = UndoKind::Structural;
    entry.beforeFlame = std::shared_ptr<apo::Flame>(before.clone());
    entry.afterFlame = std::shared_ptr<apo::Flame>(flame_->clone());
    entry.selectAfterUndo = selectAfterUndo;
    entry.selectAfterRedo = selectAfterRedo;

    undoStack_.push_back(std::move(entry));
    redoStack_.clear();
    updateUndoRedoActions();
}

void EditorWindow::onAddXform() {
    const auto before = flame_->clone();
    const int idx = flame_->addXForm();
    if (idx < 0) {
        statusBar()->showMessage("Cannot add another transform - already at the maximum");
        return;
    }

    pushStructuralUndo(*before, canvas_->selectedXform(), idx);
    canvas_->setSelectedXform(idx);
    refreshXformList();
    refreshXaosDialogIfOpen();
    requestRender();
}

void EditorWindow::onDuplicateXform() {
    const int sel = canvas_->selectedXform();
    if (sel < 0) return;

    const auto before = flame_->clone();
    const int idx = flame_->duplicateXForm(sel);
    if (idx < 0) {
        statusBar()->showMessage("Cannot duplicate - already at the maximum number of transforms");
        return;
    }

    pushStructuralUndo(*before, sel, idx);
    canvas_->setSelectedXform(idx);
    refreshXformList();
    refreshXaosDialogIfOpen();
    requestRender();
}

void EditorWindow::onDeleteXform() {
    const int sel = canvas_->selectedXform();
    if (sel < 0) return;
    if (flame_->numXForms() <= 1) {
        statusBar()->showMessage("Cannot delete the flame's last remaining transform");
        return;
    }

    const auto before = flame_->clone();
    flame_->removeXForm(sel);
    const int newSel = std::min(sel, flame_->numXForms() - 1);

    pushStructuralUndo(*before, sel, newSel);
    canvas_->setSelectedXform(newSel);
    refreshXformList();
    refreshXaosDialogIfOpen();
    requestRender();
}

void EditorWindow::onCopyXform() {
    const int sel = canvas_->selectedXform();
    if (sel < 0 && sel != apo::kFinalXformIndex) return;

    g_transformClipboard = std::make_unique<apo::XForm>();
    g_transformClipboard->assign(xformAt(sel));
    statusBar()->showMessage("Transform copied");
}

void EditorWindow::onPasteXform() {
    if (!g_transformClipboard) {
        statusBar()->showMessage("Nothing to paste - copy a transform first");
        return;
    }
    const int sel = canvas_->selectedXform();
    if (sel < 0 && sel != apo::kFinalXformIndex) return;

    const auto before = flame_->clone();
    xformAt(sel).assign(*g_transformClipboard);

    pushStructuralUndo(*before, sel, sel);
    refreshXformList();
    refreshXaosDialogIfOpen();
    requestRender();
    statusBar()->showMessage("Transform pasted");
}

void EditorWindow::refreshXaosDialogIfOpen() {
    if (xaosDialog_) xaosDialog_->refreshFromFlame();
}

void EditorWindow::onRandomizeWeights() {
    if (flame_->numXForms() == 0) return;
    const auto before = flame_->clone();
    apo::Xoshiro256Rng rng(static_cast<std::uint64_t>(std::random_device{}()));
    apo::randomizeXformWeights(*flame_, rng);

    const int sel = canvas_->selectedXform();
    pushStructuralUndo(*before, sel, sel);
    transformPanel_->setSelectedXform(sel); // re-reads the (possibly selected) xform's now-changed weight
    canvas_->update();
    requestRender();
    statusBar()->showMessage("Randomized transform weights");
}

void EditorWindow::onEqualizeWeights() {
    if (flame_->numXForms() == 0) return;
    const auto before = flame_->clone();
    apo::equalizeXformWeights(*flame_);

    const int sel = canvas_->selectedXform();
    pushStructuralUndo(*before, sel, sel);
    transformPanel_->setSelectedXform(sel);
    canvas_->update();
    requestRender();
    statusBar()->showMessage("Equalized transform weights");
}

void EditorWindow::onCalculateColorValues() {
    if (flame_->numXForms() == 0) return;
    const auto before = flame_->clone();
    apo::calculateXformColorValues(*flame_);

    const int sel = canvas_->selectedXform();
    pushStructuralUndo(*before, sel, sel);
    transformPanel_->setSelectedXform(sel);
    canvas_->update();
    requestRender();
    statusBar()->showMessage("Calculated transform color values");
}

void EditorWindow::onRandomizeColorValues() {
    if (flame_->numXForms() == 0) return;
    const auto before = flame_->clone();
    apo::Xoshiro256Rng rng(static_cast<std::uint64_t>(std::random_device{}()));
    apo::randomizeXformColorValues(*flame_, rng);

    const int sel = canvas_->selectedXform();
    pushStructuralUndo(*before, sel, sel);
    transformPanel_->setSelectedXform(sel);
    canvas_->update();
    requestRender();
    statusBar()->showMessage("Randomized transform color values");
}

void EditorWindow::openXaosDialog() {
    if (xaosDialog_) {
        xaosDialog_->raise();
        xaosDialog_->activateWindow();
        return;
    }

    // Mutates the shared flame_ in place (its xforms' modWeights) - same
    // flameChanged-signal pattern as openAdjustDialog()/openCurvesDialog().
    xaosDialog_ = new XaosDialog(flame_, this);
    connect(xaosDialog_, &XaosDialog::flameChanged, this, &EditorWindow::requestRender);
    connect(xaosDialog_, &QObject::destroyed, this, [this] { xaosDialog_ = nullptr; });
    xaosDialog_->show();
}

void EditorWindow::openForceSymmetryDialog() {
    // Fire-and-forget, unlike XaosDialog - Forced Symmetry only ever
    // *appends* xforms (see Symmetry.h), it never invalidates indices an
    // already-open instance would need to track, so there's no live-sync
    // concern requiring a persistent pointer.
    auto* dialog = new ForceSymmetryDialog(flame_, this);
    connect(dialog, &ForceSymmetryDialog::applyRequested, this, &EditorWindow::onForceSymmetryRequested);
    dialog->show();
}

void EditorWindow::onForceSymmetryRequested(int sym) {
    const auto before = flame_->clone();
    const int selBefore = canvas_->selectedXform();
    const int added = apo::addSymmetry(*flame_, sym);
    if (added <= 0) {
        statusBar()->showMessage("Force Symmetry made no changes (order too small, or already at the maximum "
                                  "number of transforms)");
        return;
    }

    // Select the first newly-appended xform, matching Add/Duplicate's own
    // "land the selection on what you just created" convention.
    const int newSel = flame_->numXForms() - added;
    pushStructuralUndo(*before, selBefore, newSel);
    canvas_->setSelectedXform(newSel);
    refreshXformList();
    refreshXaosDialogIfOpen();
    requestRender();
}

void EditorWindow::onCanvasSelectionChanged(int index) {
    xformList_->blockSignals(true);
    const int row = listRowForXformIndex(index);
    if (row >= 0 && row < xformList_->count()) xformList_->setCurrentRow(row);
    xformList_->blockSignals(false);
    transformPanel_->setSelectedXform(index);
}

void EditorWindow::onXformListSelectionChanged(int row) {
    const int index = xformIndexForListRow(row);
    canvas_->setSelectedXform(index);
    transformPanel_->setSelectedXform(index);
}

void EditorWindow::onFinalXformToggled(bool enabled) {
    const auto before = flame_->clone();
    const int selBefore = canvas_->selectedXform();

    flame_->finalXformEnabled = enabled;
    flame_->useFinalXform = enabled;

    // Enabling lands the selection on the final transform, matching Add/
    // Duplicate/Force Symmetry's own "select what you just created"
    // convention; disabling falls back to xform 0 only if the final
    // transform itself was the one selected (otherwise leave whatever
    // regular xform was already selected alone).
    int selAfter = selBefore;
    if (enabled) {
        selAfter = apo::kFinalXformIndex;
    } else if (selBefore == apo::kFinalXformIndex) {
        selAfter = flame_->numXForms() > 0 ? 0 : -1;
    }

    pushStructuralUndo(*before, selBefore, selAfter);
    canvas_->setSelectedXform(selAfter);
    refreshXformList();
    requestRender();
}

void EditorWindow::requestRender() {
    if (renderInFlight_) {
        renderDirty_ = true;
        return;
    }
    renderInFlight_ = true;
    statusBar()->showMessage("Rendering...");

    // Preview clone, not flame_ itself - see this header's class comment
    // for why rendering flame_ as-is (its own declared width/height/
    // sampleDensity) on every single triangle-drag mouse-move was a real
    // performance problem, not just a missing nicety.
    auto previewFlame = flame_->clone();
    const int pw = std::max(canvas_->width(), 64);
    const int ph = std::max(canvas_->height(), 64);
    previewFlame->adjustScale(pw, ph);
    previewFlame->sampleDensity = AppSettings::previewSampleDensity();

    std::shared_ptr<const apo::Flame> shared(std::move(previewFlame));
    emit renderRequested(shared, /*seed=*/1);
}

void EditorWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    requestRender();
}

void EditorWindow::onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted) {
    canvas_->setBackgroundImage(image);

    const double acceptedPct =
        pointsGenerated > 0 ? 100.0 * static_cast<double>(pointsAccepted) / static_cast<double>(pointsGenerated) : 0.0;
    statusBar()->showMessage(
        QString("Rendered %1x%2 - %3 points, %4% accepted").arg(image.width()).arg(image.height()).arg(pointsGenerated).arg(acceptedPct, 0, 'f', 1));

    renderInFlight_ = false;
    if (renderDirty_) {
        renderDirty_ = false;
        requestRender();
        return;
    }

    if (!autoScreenshotPath_.isEmpty()) {
        // Deferred to the next event-loop iteration so the canvas has
        // actually repainted with the new background before it's grabbed
        // - see MainWindow::onRenderFinished's identical reasoning.
        const QString path = autoScreenshotPath_;
        const bool exitAfter = autoScreenshotExit_;
        autoScreenshotPath_.clear();
        QTimer::singleShot(100, this, [this, path, exitAfter] {
            grab().save(path, "PNG");
            if (exitAfter) qApp->quit();
        });
    }
}

void EditorWindow::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void EditorWindow::openAdjustDialog() {
    // A fresh dialog per click (WA_DeleteOnClose handles cleanup), same
    // pattern MainWindow uses for opening EditorWindow itself - both edit
    // the same shared Flame, so there's no state to lose by not reusing a
    // single persistent instance.
    auto* dialog = new AdjustDialog(flame_, this);
    connect(dialog, &AdjustDialog::flameChanged, this, &EditorWindow::requestRender);
    dialog->show();
}

void EditorWindow::openRenderDialog() {
    // RenderDialog renders its own clone of flame_ (see its header comment)
    // rather than mutating the shared one, so unlike openAdjustDialog()
    // there's no flameChanged-style signal to connect back here.
    auto* dialog = new RenderDialog(flame_, this);
    dialog->show();
}

void EditorWindow::openMutateDialog() {
    // MutateDialog mutates the shared flame_ in place when a mutant is
    // adopted (see Flame::copyFrom) - same flameChanged-signal pattern as
    // openAdjustDialog().
    auto* dialog = new MutateDialog(flame_, this);
    connect(dialog, &MutateDialog::flameChanged, this, &EditorWindow::requestRender);
    dialog->show();
}

void EditorWindow::openCurvesDialog() {
    // CurvesDialog mutates the shared flame_ in place (its curves field) -
    // same flameChanged-signal pattern as openAdjustDialog()/openMutateDialog().
    auto* dialog = new CurvesDialog(flame_, this);
    connect(dialog, &CurvesDialog::flameChanged, this, &EditorWindow::requestRender);
    dialog->show();
}

void EditorWindow::openPostProcessDialog() {
    // PostProcessDialog clones flame_ internally and never writes back to
    // it (see PostProcessDialog.h's class comment) - unlike Adjust/Mutate/
    // Curves, there is no flameChanged signal to connect here.
    auto* dialog = new PostProcessDialog(flame_, this);
    dialog->show();
}

void EditorWindow::onSaveFlameAsTriggered() {
    const QString suggested = flame_->name.empty() ? "untitled.flame" : QString::fromStdString(flame_->name) + ".flame";
    const QString path = QFileDialog::getSaveFileName(this, "Save Flame As", suggested, "Flame files (*.flame)");
    if (path.isEmpty()) return;

    if (!apo::saveFlameFile(path.toStdString(), {flame_.get()})) {
        QMessageBox::warning(this, "Save Flame", "Failed to write:\n" + path);
    } else {
        statusBar()->showMessage("Saved " + path);
    }
}

void EditorWindow::openFullscreenView() {
    // Renders its own clone of flame_ at screen resolution (see
    // FullscreenView.h) - a view-only window, so unlike Adjust/Mutate
    // there's nothing to write back to flame_ here.
    auto* view = new FullscreenView(flame_, this);
    view->showFullScreen();
}

} // namespace apo::ui
