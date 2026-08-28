#include "EditorWindow.h"

#include <algorithm>
#include <random>

#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include "AdjustPanel.h"
#include "AppSettings.h"
#include "CurvesDialog.h"
#include "FileDialogSupport.h"
#include "ForceSymmetryDialog.h"
#include "FullscreenView.h"
#include "MutateDialog.h"
#include "PreviewSizing.h"
#include "RenderDialog.h"
#include "RenderWorker.h"
#include "TransformPanel.h"
#include "TriangleCanvas.h"
#include "WindowGeometry.h"
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
    connect(transformPanel_, &TransformPanel::propertyEdited, this, [this] { requestRender(); });
    connect(transformPanel_, &TransformPanel::editingStarted, this, &EditorWindow::onXformPropertyEditingStarted);
    connect(transformPanel_, &TransformPanel::editingFinished, this, &EditorWindow::onXformPropertyEditingFinished);
    connect(transformPanel_, &TransformPanel::descriptionsVisibilityChanged, this,
            &EditorWindow::onDescriptionsVisibilityChanged);

    // Second page of the same right-hand panel switcher (see this class's
    // own header comment) - AdjustPanel mutates the same shared flame_ and
    // feeds the exact same undo path as transformPanel_ above, just via its
    // own propertyEdited/editingStarted/editingFinished signals (its
    // contract is deliberately identical to TransformPanel's - see
    // AdjustPanel.h).
    adjustPanel_ = new AdjustPanel(flame_, this);
    connect(adjustPanel_, &AdjustPanel::propertyEdited, this, [this] { requestRender(); });
    connect(adjustPanel_, &AdjustPanel::editingStarted, this, &EditorWindow::onXformPropertyEditingStarted);
    connect(adjustPanel_, &AdjustPanel::editingFinished, this, &EditorWindow::onXformPropertyEditingFinished);

    refreshXformList();

    rightTabBar_ = new QTabBar(this);
    rightTabBar_->setObjectName("rightPanelTabBar");
    rightTabBar_->addTab("Transform");
    rightTabBar_->addTab("Adjust");
    connect(rightTabBar_, &QTabBar::currentChanged, this, &EditorWindow::onRightPanelTabChanged);

    rightStack_ = new QStackedWidget(this);
    rightStack_->addWidget(transformPanel_);
    rightStack_->addWidget(adjustPanel_);

    auto* rightContainer = new QWidget(this);
    rightContainer->setMinimumWidth(280);
    rightContainer->setMaximumWidth(900);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(rightTabBar_);
    rightLayout->addWidget(rightStack_, 1);

    // xformList_ stays fixed-width (it's just xform names, nothing to gain
    // from resizing it) but canvas_/rightContainer share a QSplitter so the
    // transform/adjust panel can be dragged wider - TransformPanel's
    // Variations table hosts a Description column (see its own
    // buildVariationsTab) that needs more room than the 280px default to
    // read comfortably. setSizes() below seeds the same split the old
    // fixed layout used (canvas gets whatever's left, panel starts at 280)
    // so the window looks identical on open; the stretch factors then keep
    // that same "canvas absorbs extra space" behavior on window resize once
    // the user hasn't dragged the handle themselves.
    xformList_->setFixedWidth(150);

    centralSplitter_ = new QSplitter(Qt::Horizontal, this);
    centralSplitter_->setObjectName("centralSplitter");
    centralSplitter_->setChildrenCollapsible(false);
    centralSplitter_->addWidget(canvas_);
    centralSplitter_->addWidget(rightContainer);
    centralSplitter_->setStretchFactor(0, 1);
    centralSplitter_->setStretchFactor(1, 0);
    centralSplitter_->setSizes({720, 280});

    QWidget* central = new QWidget(this);
    QHBoxLayout* centralLayout = new QHBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(xformList_);
    centralLayout->addWidget(centralSplitter_, 1);
    setCentralWidget(central);

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
    editMenu->addSeparator();
    QAction* copyXformAction = editMenu->addAction("&Copy Transform");
    copyXformAction->setObjectName("copyXformAction");
    copyXformAction->setToolTip("Copy the selected transform (coefficients, variations, colors, name)");
    connect(copyXformAction, &QAction::triggered, this, &EditorWindow::onCopyXform);
    QAction* pasteXformAction = editMenu->addAction("&Paste Transform");
    pasteXformAction->setObjectName("pasteXformAction");
    pasteXformAction->setToolTip("Paste the last-copied transform onto the selected one");
    connect(pasteXformAction, &QAction::triggered, this, &EditorWindow::onPasteXform);
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
    flameMenu->addSeparator();
    finalXformAction_ = flameMenu->addAction("&Final Transform");
    finalXformAction_->setObjectName("finalXformAction");
    finalXformAction_->setCheckable(true);
    finalXformAction_->setToolTip("A transform applied once, after every iteration - unlike a regular "
                                   "transform, it's never picked probabilistically");
    connect(finalXformAction_, &QAction::toggled, this, &EditorWindow::onFinalXformToggled);
    QAction* xaosAction = flameMenu->addAction("&Xaos...");
    xaosAction->setObjectName("xaosAction");
    connect(xaosAction, &QAction::triggered, this, &EditorWindow::openXaosDialog);
    QAction* forceSymmetryAction = flameMenu->addAction("Force S&ymmetry...");
    forceSymmetryAction->setObjectName("forceSymmetryAction");
    connect(forceSymmetryAction, &QAction::triggered, this, &EditorWindow::openForceSymmetryDialog);
    flameMenu->addSeparator();
    QAction* mutateAction = flameMenu->addAction("Mu&tate...");
    connect(mutateAction, &QAction::triggered, this, &EditorWindow::openMutateDialog);
    QAction* curvesAction = flameMenu->addAction("Cu&rves...");
    connect(curvesAction, &QAction::triggered, this, &EditorWindow::openCurvesDialog);
    QAction* fullscreenAction = flameMenu->addAction("F&ullscreen");
    connect(fullscreenAction, &QAction::triggered, this, &EditorWindow::openFullscreenView);

    QToolBar* toolbar = addToolBar("Edit");
    toolbar->setMovable(false);
    // The default checked-state look for a QToolButton (a thin, easy-to-
    // miss sunken border) wasn't visible enough to tell which of Move/
    // Rotate/Scale/Post is currently active - palette(highlight) matches
    // whatever accent color the OS theme actually uses (works in both
    // light and dark themes), so the active one now gets an unmistakable
    // filled background instead.
    toolbar->setStyleSheet(
        "QToolButton:checked {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: 1px solid palette(highlight);"
        "}");

    QAction* adjustAction = toolbar->addAction("Adjust");
    adjustAction->setObjectName("adjustAction");
    adjustAction->setToolTip("Camera/Coloring/Gradient/Size controls - switches the right-hand panel to Adjust");
    connect(adjustAction, &QAction::triggered, this, [this] { rightTabBar_->setCurrentIndex(1); });
    QAction* renderAction = toolbar->addAction("Render...");
    connect(renderAction, &QAction::triggered, this, &EditorWindow::openRenderDialog);
    toolbar->addSeparator();

    auto* modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    moveAction_ = toolbar->addAction("Move");
    moveAction_->setObjectName("moveAction");
    moveAction_->setCheckable(true);
    moveAction_->setChecked(true);
    // Escape always gets you back to Move, regardless of the triangle
    // canvas's own automatic reset-on-selection-change (see
    // TriangleCanvas::setSelectedXform) - an explicit way out of
    // Rotate/Scale without having to switch triangles first.
    moveAction_->setShortcut(QKeySequence(Qt::Key_Escape));
    modeGroup->addAction(moveAction_);
    connect(moveAction_, &QAction::triggered, this, [this] { canvas_->setEditMode(TriangleCanvas::EditMode::Move); });

    rotateAction_ = toolbar->addAction("Rotate");
    rotateAction_->setObjectName("rotateAction");
    rotateAction_->setCheckable(true);
    modeGroup->addAction(rotateAction_);
    connect(rotateAction_, &QAction::triggered, this,
            [this] { canvas_->setEditMode(TriangleCanvas::EditMode::Rotate); });

    scaleAction_ = toolbar->addAction("Scale");
    scaleAction_->setObjectName("scaleAction");
    scaleAction_->setCheckable(true);
    modeGroup->addAction(scaleAction_);
    connect(scaleAction_, &QAction::triggered, this,
            [this] { canvas_->setEditMode(TriangleCanvas::EditMode::Scale); });

    // A permanent (never overwritten by statusBar()->showMessage(), unlike
    // the transient "Rendering..."/"Ready" text sharing the same status
    // bar) readout of the current mode - the toolbar highlight above is
    // easy to miss at a glance, this spells it out in words.
    modeLabel_ = new QLabel(statusBar());
    modeLabel_->setObjectName("modeLabel");
    modeLabel_->setStyleSheet("font-weight: bold; padding: 0 6px;");
    modeLabel_->setText("Mode: Move");
    statusBar()->addPermanentWidget(modeLabel_);

    // Reverse sync: canvas_->setEditMode() can now change the mode on its
    // own (the reset-to-Move on xform selection change) as well as via
    // these actions' own triggered handlers above - this keeps whichever
    // toolbar button is checked (and modeLabel_'s text) always matching
    // the canvas's real mode, no matter which side initiated the change.
    // Mirrors the postAction/editingPostTransformChanged two-way binding
    // below.
    connect(canvas_, &TriangleCanvas::editModeChanged, this, [this](TriangleCanvas::EditMode mode) {
        switch (mode) {
            case TriangleCanvas::EditMode::Move:
                moveAction_->setChecked(true);
                modeLabel_->setText("Mode: Move");
                break;
            case TriangleCanvas::EditMode::Rotate:
                rotateAction_->setChecked(true);
                modeLabel_->setText("Mode: Rotate");
                break;
            case TriangleCanvas::EditMode::Scale:
                scaleAction_->setChecked(true);
                modeLabel_->setText("Mode: Scale");
                break;
        }
    });

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

    // Matches Main.dfm's tbQualityBox - see onQualityBoxCommitted()'s doc
    // comment for what it controls here specifically.
    toolbar->addSeparator();
    qualityBox_ = new QComboBox(toolbar);
    qualityBox_->setObjectName("qualityBox");
    qualityBox_->setEditable(true);
    qualityBox_->setInsertPolicy(QComboBox::NoInsert);
    qualityBox_->addItems({"5", "10", "15", "25", "50", "100", "150", "250", "500", "1000"});
    qualityBox_->setCurrentText(QString::number(AppSettings::previewSampleDensity()));
    qualityBox_->setFixedWidth(65);
    qualityBox_->setToolTip("Preview render quality (sample density)");
    connect(qualityBox_, &QComboBox::activated, this, &EditorWindow::onQualityBoxCommitted);
    connect(qualityBox_->lineEdit(), &QLineEdit::editingFinished, this, &EditorWindow::onQualityBoxCommitted);
    toolbar->addWidget(qualityBox_);

    statusBar()->showMessage("Ready");

    // onProgressTick()'s poll interval - matches MainWindow's identical timer.
    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(150);
    connect(progressTimer_, &QTimer::timeout, this, &EditorWindow::onProgressTick);

    // Same dedicated-worker-thread pattern as MainWindow (see
    // RenderWorker.h) - but here, driven by triangle drags rather than a
    // one-shot file open, so requestRender() coalesces bursts of
    // xformEdited signals (one per mouse-move) into "at most one render in
    // flight, always eventually reflecting the latest state" rather than
    // queuing every intermediate frame.
    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &EditorWindow::renderRequested, worker_, &RenderWorker::renderFlameWithProgress);
    connect(worker_, &RenderWorker::renderFinished, this, &EditorWindow::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    // See WindowGeometry.h's restoreWindowGeometry() doc comment: must come
    // after the full UI/layout is built, not right after the resize() above.
    restoreWindowGeometry(this, "EditorWindow");
    // centralSplitter_'s own saved-width restore deliberately does NOT
    // happen here - see showEvent()'s doc comment for why a hidden
    // top-level widget's own geometry restoring correctly (immediately
    // above) doesn't mean its *children*'s layout has caught up yet, and
    // why that gap can't be reliably closed from inside the constructor.
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

void EditorWindow::onDescriptionsVisibilityChanged(bool show) {
    // Keeps the canvas's own current width, only resizing the panel itself
    // - so this doesn't fight a size the user already dragged to on the
    // canvas side, just grows/shrinks the panel to fit (or stop needing)
    // the Description column. 520px comfortably fits the longest built-in
    // description without wrapping; 280px matches the panel's original
    // (pre-splitter) fixed width for the compact Name/Value-only view.
    const QList<int> current = centralSplitter_->sizes();
    const int canvasWidth = current.value(0);
    const int panelWidth = show ? 520 : 280;
    centralSplitter_->setSizes({canvasWidth, panelWidth});
}

void EditorWindow::onRightPanelTabChanged(int index) {
    rightStack_->setCurrentIndex(index);
    if (index == 1) adjustPanel_->refreshControlsFromFlame();
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
        // A Structural entry can carry an AdjustPanel edit (camera/coloring/
        // gradient/size are flame-level, not per-xform) - refresh
        // unconditionally, matching refreshXformList()'s own
        // "always resync, whether or not this panel happens to be visible
        // right now" contract for transformPanel_ above.
        adjustPanel_->refreshControlsFromFlame();
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
        adjustPanel_->refreshControlsFromFlame();
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
    // flameChanged-signal pattern as openCurvesDialog().
    xaosDialog_ = new XaosDialog(flame_, this);
    connect(xaosDialog_, &XaosDialog::flameChanged, this, [this] { requestRender(); });
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

void EditorWindow::onQualityBoxCommitted() {
    bool ok = false;
    const double density = qualityBox_->currentText().toDouble(&ok);
    if (!ok || density <= 0.0) {
        qualityBox_->setCurrentText(QString::number(AppSettings::previewSampleDensity()));
        return;
    }
    AppSettings::setPreviewSampleDensity(density);
    requestRender(/*trackProgress=*/true);
}

void EditorWindow::requestRender(bool trackProgress) {
    if (renderInFlight_) {
        renderDirty_ = true;
        pendingTrackProgress_ = pendingTrackProgress_ || trackProgress;
        // Cooperatively cancel whatever's currently rendering (same
        // mechanism RenderDialog's Cancel button uses) instead of just
        // queuing behind it - the worker's Renderer::render() checks
        // cancelRequested every sub-batch and unwinds early, so
        // onRenderFinished() fires promptly and, seeing renderDirty_, kicks
        // off this newer request immediately rather than after the
        // stale one runs to completion.
        if (progress_) progress_->cancelRequested.store(true, std::memory_order_relaxed);
        statusBar()->showMessage("Cancelling preview render...");
        return;
    }
    renderInFlight_ = true;
    statusBar()->showMessage("Rendering...");

    // Preview clone, not flame_ itself - see this header's class comment
    // for why rendering flame_ as-is (its own declared width/height/
    // sampleDensity) on every single triangle-drag mouse-move was a real
    // performance problem, not just a missing nicety.
    auto previewFlame = flame_->clone();
    int pw, ph;
    fitPreviewSize(canvas_->width(), canvas_->height(), flame_->width, flame_->height, pw, ph);
    previewFlame->adjustScale(pw, ph);
    previewFlame->sampleDensity = AppSettings::previewSampleDensity();

    std::shared_ptr<const apo::Flame> shared(std::move(previewFlame));

    // Always a fresh, cancellable token now - see this function's own doc
    // comment in EditorWindow.h.
    progress_ = std::make_unique<apo::RenderProgress>();
    if (trackProgress) {
        elapsedTimer_.start();
        progressTimer_->start();
    } else {
        progressTimer_->stop();
    }
    emit renderRequested(shared, /*seed=*/1, progress_.get());
}

void EditorWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    requestRender();
}

// Restores a width the user previously dragged centralSplitter_'s handle to
// (see AppSettings::splitterState's own doc comment) - deliberately not
// done back in the constructor. restoreWindowGeometry() there does give
// *this* widget its own correct, final size immediately, even while still
// hidden - but centralSplitter_ is two layouts down (QMainWindowLayout's
// central-widget slot, then the central widget's own QHBoxLayout), and Qt
// doesn't guarantee that a hidden top-level widget's *children* have
// caught up to a geometry change yet: QSplitter::restoreState() would then
// have nothing real to apply a saved width against, so the saved sizes get
// silently discarded in favor of whatever the next actual resize computes
// from the stretch factors instead. Various ways of forcing that gap closed
// synchronously from inside the constructor (flushing pending layout
// events, calling layout()->activate() directly) worked in some manual/local
// testing but still reproduced the same discarded-width failure in CI on
// both Linux and macOS (both running tests under the offscreen QPA
// plugin) - evidently the gap isn't reliably closeable pre-show() at all,
// on at least some Qt-version/QPA-plugin combinations. showEvent() is the
// one point Qt itself documents as guaranteed to fire only once the
// widget's full layout, size, and children are final and about to be
// painted, so restoring here - once, guarded by splitterStateRestored_ so a
// later re-show (e.g. after minimize) doesn't stomp a width the user has
// since dragged - is reliable regardless of platform/QPA-plugin timing.
// Confirmed via tests/ui/editor_splitter_test.cpp.
void EditorWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (!splitterStateRestored_) {
        splitterStateRestored_ = true;
        centralSplitter_->restoreState(AppSettings::splitterState("EditorWindow"));
    }
}

void EditorWindow::closeEvent(QCloseEvent* event) {
    saveWindowGeometry(this, "EditorWindow");
    AppSettings::setSplitterState("EditorWindow", centralSplitter_->saveState());
    QMainWindow::closeEvent(event);
}

void EditorWindow::onProgressTick() {
    if (!progress_) return;

    const std::uint64_t done = progress_->pointsDone.load(std::memory_order_relaxed);
    const std::uint64_t target = progress_->pointsTarget.load(std::memory_order_relaxed);
    const double prog = target > 0 ? static_cast<double>(done) / static_cast<double>(target) : 0.0;
    const int percent = static_cast<int>(std::min<std::uint64_t>(100, target > 0 ? done * 100 / target : 0));

    const double elapsedSec = elapsedTimer_.elapsed() / 1000.0;
    // Same elapsed/(remaining = elapsed/progress - elapsed) formula as
    // MainWindow::onProgressTick - see its own doc comment for where this
    // comes from.
    const double remainingSec = prog > 0.0 ? (elapsedSec / prog - elapsedSec) : 0.0;

    statusBar()->showMessage(QString("Rendering... %1% (%2s elapsed, ~%3s remaining)")
                                  .arg(percent)
                                  .arg(elapsedSec, 0, 'f', 1)
                                  .arg(remainingSec, 0, 'f', 1));
}

void EditorWindow::onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted) {
    progressTimer_->stop();
    progress_.reset();
    renderInFlight_ = false;

    if (renderDirty_) {
        // A newer render was requested (and this one cooperatively
        // cancelled, if it hadn't already finished on its own - see
        // requestRender()'s doc comment) while this one was in flight, so
        // `image` is stale or only partially rendered - skip displaying it
        // and go straight to the render that's actually still wanted,
        // rather than flashing a half-finished preview first.
        renderDirty_ = false;
        const bool trackProgress = pendingTrackProgress_;
        pendingTrackProgress_ = false;
        requestRender(trackProgress);
        return;
    }

    canvas_->setBackgroundImage(image);

    const double acceptedPct =
        pointsGenerated > 0 ? 100.0 * static_cast<double>(pointsAccepted) / static_cast<double>(pointsGenerated) : 0.0;
    statusBar()->showMessage(
        QString("Rendered %1x%2 - %3 points, %4% accepted").arg(image.width()).arg(image.height()).arg(pointsGenerated).arg(acceptedPct, 0, 'f', 1));

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

void EditorWindow::openRenderDialog() {
    // RenderDialog renders its own clone of flame_ (see its header comment)
    // rather than mutating the shared one, so unlike adjustPanel_ there's no
    // flameChanged-style signal to connect back here.
    auto* dialog = new RenderDialog(flame_, this);
    dialog->show();
}

void EditorWindow::openMutateDialog() {
    // MutateDialog mutates the shared flame_ in place when a mutant is
    // adopted (see Flame::copyFrom) - same flameChanged-signal pattern as
    // adjustPanel_'s propertyEdited/editingFinished.
    auto* dialog = new MutateDialog(flame_, this);
    connect(dialog, &MutateDialog::flameChanged, this, [this] { requestRender(); });
    dialog->show();
}

void EditorWindow::openCurvesDialog() {
    // CurvesDialog mutates the shared flame_ in place (its curves field) -
    // same flameChanged-signal pattern as openMutateDialog().
    auto* dialog = new CurvesDialog(flame_, this);
    connect(dialog, &CurvesDialog::flameChanged, this, [this] { requestRender(); });
    dialog->show();
}

void EditorWindow::onSaveFlameAsTriggered() {
    const QString suggested = flame_->name.empty() ? "untitled.flame" : QString::fromStdString(flame_->name) + ".flame";
    const QString path = QFileDialog::getSaveFileName(this, "Save Flame As", suggested, "Flame files (*.flame)",
                                                       nullptr, testFriendlyFileDialogOptions());
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
