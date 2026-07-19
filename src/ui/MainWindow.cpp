#include "MainWindow.h"

#include <algorithm>
#include <random>

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QResizeEvent>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QToolBar>
#include <QUrl>

#include "AboutDialog.h"
#include "AppSettings.h"
#include "EditorWindow.h"
#include "OptionsDialog.h"
#include "PreviewLabel.h"
#include "RenderAllDialog.h"
#include "RenderWorker.h"
#include "SmoothPaletteDialog.h"
#include "ThumbnailTask.h"
#include "core/BuiltinGradients.h"
#include "core/VariationRegistry.h"
#include "core/edit/RandomFlame.h"
#include "core/edit/Symmetry.h"
#include "core/render/AutoFrame.h"
#include "core/io/FlameIO.h"

namespace apo::ui {

namespace {
constexpr int kThumbnailSize = 64;

// Options.pas's Display tab "Rendering" group (see AppSettings.h) - applied
// to every brand-new flame (New Flame, and each flame in a random batch)
// instead of leaving them at Flame's own class defaults.
void applyDefaultRenderSettings(apo::Flame& flame) {
    flame.gamma = AppSettings::defaultGamma();
    flame.brightness = AppSettings::defaultBrightness();
    flame.vibrancy = AppSettings::defaultVibrancy();
    flame.gammaThreshold = AppSettings::defaultGammaThreshold();
    flame.sampleDensity = AppSettings::defaultSampleDensity();
    flame.spatialOversample = AppSettings::defaultOversample();
    flame.spatialFilterRadius = AppSettings::defaultFilterRadius();
}

// "New Flame"'s starting point - matches the *intent* of Template.pas's
// blankFlameXML (a single default xform, ready to edit) but not its literal
// content: that template's palette is all-black (RGB 0,0,0 for all 256
// entries), presumably meant as a truly blank canvas, but that's a poor
// first impression for a brand-new user - a real starter gradient (the
// first built-in one) is a more useful default to actually see something
// while editing. The curated multi-template picker Template.pas otherwise
// offered isn't ported - its template library file isn't present in this
// port's source tree (see the Phase 7 scoping notes), only its "blank"
// entry is really reproducible here.
std::shared_ptr<apo::Flame> makeBlankFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->name = "Untitled";
    flame->width = 640;
    flame->height = 480;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    if (apo::builtinGradientCount() > 0) flame->cmap = apo::builtinGradient(0);
    applyDefaultRenderSettings(*flame);
    return flame;
}

// A4's undo entries want shared_ptr<Flame> snapshots (Flame::clone()
// itself returns unique_ptr<Flame> - see its own doc comment).
std::shared_ptr<apo::Flame> cloneShared(const apo::Flame& flame) { return std::shared_ptr<apo::Flame>(flame.clone()); }
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Apophysis 7X");
    resize(1100, 750);
    setAcceptDrops(true); // A7 - see dragEnterEvent/dropEvent below

    flameList_ = new QListWidget(this);
    flameList_->setResizeMode(QListView::Adjust);
    flameList_->setMovement(QListView::Static);
    flameList_->setMaximumWidth(260);
    flameList_->setMinimumWidth(160);
    // EditKeyPressed only (not the Qt default's DoubleClicked too) -
    // double-click already means "open in Editor" (onItemActivated below);
    // renaming happens via F2/the context menu instead, so the two never
    // race on the same click.
    flameList_->setEditTriggers(QAbstractItemView::EditKeyPressed);
    flameList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(flameList_, &QListWidget::currentRowChanged, this, &MainWindow::onSelectionChanged);
    // itemActivated (not itemDoubleClicked) - covers double-click *and*
    // pressing Enter/Return with an item current, in one connection, so a
    // batch-generated flame is exactly as openable as a file-opened one
    // via either gesture, and there's no risk of double-click firing this
    // twice (it would if both itemDoubleClicked and itemActivated were
    // connected here, since itemActivated already fires on double-click
    // too).
    connect(flameList_, &QListWidget::itemActivated, this, &MainWindow::onItemActivated);
    connect(flameList_, &QListWidget::customContextMenuRequested, this,
            &MainWindow::onFlameListContextMenuRequested);
    connect(flameList_, &QListWidget::itemChanged, this, &MainWindow::onFlameItemChanged);

    previewLabel_ = new PreviewLabel(this);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(200, 200);
    previewLabel_->setStyleSheet("background-color: #000;");
    connect(previewLabel_, &PreviewLabel::gestureStarted, this, &MainWindow::onCameraGestureStarted);
    connect(previewLabel_, &PreviewLabel::cameraChanged, this, &MainWindow::onCameraChanged);
    connect(previewLabel_, &PreviewLabel::cameraChangeFinished, this, &MainWindow::onCameraChangeFinished);

    QSplitter* splitter = new QSplitter(this);
    splitter->addWidget(flameList_);
    splitter->addWidget(previewLabel_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    QAction* newAction = new QAction("&New Flame", this);
    newAction->setObjectName("newFlameAction");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::onNewFlameTriggered);

    QAction* newRandomBatchAction = new QAction("New &Random Batch...", this);
    newRandomBatchAction->setObjectName("newRandomBatchAction");
    // Matches Main.dfm's mnuRandomBatch (ShortCut = 16450 = Ctrl+B in
    // Delphi's packed VK-code+modifier TShortCut encoding, decoded and
    // verified against several neighboring entries' own known shortcuts).
    newRandomBatchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(newRandomBatchAction, &QAction::triggered, this, &MainWindow::onNewRandomBatchTriggered);

    QAction* openAction = new QAction("&Open...", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenTriggered);

    QAction* saveFlameAction = new QAction("Save &Flame As...", this);
    saveFlameAction->setObjectName("saveFlameAsAction");
    saveFlameAction->setShortcut(QKeySequence::Save);
    connect(saveFlameAction, &QAction::triggered, this, &MainWindow::onSaveFlameAsTriggered);

    // Matches Main.dfm's mnuSaveAllAs - see onSaveAllFlamesTriggered().
    QAction* saveAllFlamesAction = new QAction("Save All &Parameters...", this);
    saveAllFlamesAction->setObjectName("saveAllFlamesAction");
    connect(saveAllFlamesAction, &QAction::triggered, this, &MainWindow::onSaveAllFlamesTriggered);

    QAction* saveAction = new QAction("&Save Render As...", this);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveRenderTriggered);

    // Matches Main.dfm's mnuRenderAll - see onRenderAllFlamesTriggered()'s
    // own doc comment.
    QAction* renderAllAction = new QAction("Render All &Flames...", this);
    renderAllAction->setObjectName("renderAllFlamesAction");
    connect(renderAllAction, &QAction::triggered, this, &MainWindow::onRenderAllFlamesTriggered);

    // A5 - matches Main.dfm's mnuSmoothGradient - see
    // onSmoothPaletteTriggered()'s own doc comment.
    smoothPaletteAction_ = new QAction("&Smooth Palette...", this);
    smoothPaletteAction_->setObjectName("smoothPaletteAction");
    smoothPaletteAction_->setEnabled(false);
    connect(smoothPaletteAction_, &QAction::triggered, this, &MainWindow::onSmoothPaletteTriggered);

    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(newAction);
    fileMenu->addAction(newRandomBatchAction);
    fileMenu->addAction(openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveFlameAction);
    fileMenu->addAction(saveAllFlamesAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(renderAllAction);
    fileMenu->addAction(smoothPaletteAction_);
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // A4 - see onUndo/onRedo's own doc comment on scope.
    undoAction_ = new QAction("&Undo", this);
    undoAction_->setObjectName("undoAction");
    undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setEnabled(false);
    connect(undoAction_, &QAction::triggered, this, &MainWindow::onUndo);

    redoAction_ = new QAction("&Redo", this);
    redoAction_->setObjectName("redoAction");
    redoAction_->setShortcut(QKeySequence::Redo);
    redoAction_->setEnabled(false);
    connect(redoAction_, &QAction::triggered, this, &MainWindow::onRedo);

    // Matches Main.dfm's mnuCopy/mnuPaste - see onCopyFlameTriggered's own
    // doc comment for how Paste's target (a library list, not a single
    // open flame) differs from the original.
    copyFlameAction_ = new QAction("&Copy Flame", this);
    copyFlameAction_->setObjectName("copyFlameAction");
    copyFlameAction_->setShortcut(QKeySequence::Copy);
    copyFlameAction_->setEnabled(false);
    connect(copyFlameAction_, &QAction::triggered, this, &MainWindow::onCopyFlameTriggered);

    QAction* pasteFlameAction = new QAction("&Paste Flame", this);
    pasteFlameAction->setObjectName("pasteFlameAction");
    pasteFlameAction->setShortcut(QKeySequence::Paste);
    connect(pasteFlameAction, &QAction::triggered, this, &MainWindow::onPasteFlameTriggered);

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoAction_);
    editMenu->addAction(redoAction_);
    editMenu->addSeparator();
    editMenu->addAction(copyFlameAction_);
    editMenu->addAction(pasteFlameAction);

    // Explicit, discoverable equivalent of double-click/Enter - same
    // openFlameInEditor() call onItemActivated makes, just reachable from
    // the Library menu or the list's right-click context menu without
    // needing to know the double-click/Enter gesture exists at all. This
    // opens whichever flame is selected, whether it came from a file
    // (openFlameFile) or a generated batch (onNewRandomBatchTriggered) -
    // both populate the same flames_ list the same way.
    editFlameAction_ = new QAction("&Edit Flame", this);
    editFlameAction_->setObjectName("editFlameAction");
    editFlameAction_->setEnabled(false);
    connect(editFlameAction_, &QAction::triggered, this, &MainWindow::onEditFlameTriggered);

    // Not a port of an existing original menu item - "Duplicate" only
    // exists at the transform level in the original (Editor.pas, already
    // ported as EditorWindow's own Duplicate xform action), never at the
    // flame/library level in Main.pas (confirmed absent from Main.dfm/
    // Main.pas). A library-level duplicate is still a natural, low-cost
    // addition given Flame::clone() already exists and every other list-
    // management operation (Rename/Delete) is already here.
    duplicateFlameAction_ = new QAction("Dup&licate Flame", this);
    duplicateFlameAction_->setObjectName("duplicateFlameAction");
    duplicateFlameAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateFlameAction_->setEnabled(false);
    connect(duplicateFlameAction_, &QAction::triggered, this, &MainWindow::onDuplicateFlameTriggered);

    renameFlameAction_ = new QAction("Re&name Flame", this);
    renameFlameAction_->setObjectName("renameFlameAction");
    renameFlameAction_->setShortcut(QKeySequence(Qt::Key_F2));
    renameFlameAction_->setEnabled(false);
    connect(renameFlameAction_, &QAction::triggered, this, &MainWindow::onRenameFlameTriggered);

    deleteFlameAction_ = new QAction("&Delete Flame", this);
    deleteFlameAction_->setObjectName("deleteFlameAction");
    deleteFlameAction_->setShortcut(QKeySequence::Delete);
    deleteFlameAction_->setEnabled(false);
    connect(deleteFlameAction_, &QAction::triggered, this, &MainWindow::onDeleteFlameTriggered);

    // Matches Main.dfm's mnuResetLocation exactly (ShortCut = 123 = F12,
    // no modifiers).
    resetLocationAction_ = new QAction("&Reset Location", this);
    resetLocationAction_->setObjectName("resetLocationAction");
    resetLocationAction_->setShortcut(QKeySequence(Qt::Key_F12));
    resetLocationAction_->setEnabled(false);
    connect(resetLocationAction_, &QAction::triggered, this, &MainWindow::onResetLocationTriggered);

    QMenu* libraryMenu = menuBar()->addMenu("&Library");
    libraryMenu->addAction(editFlameAction_);
    libraryMenu->addAction(duplicateFlameAction_);
    libraryMenu->addAction(renameFlameAction_);
    libraryMenu->addAction(deleteFlameAction_);
    libraryMenu->addSeparator();
    libraryMenu->addAction(resetLocationAction_);

    QAction* optionsAction = new QAction("&Options...", this);
    connect(optionsAction, &QAction::triggered, this, &MainWindow::openOptionsDialog);
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction(optionsAction);

    QAction* aboutAction = new QAction("&About Apophysis 7X", this);
    aboutAction->setObjectName("aboutAction");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::openAboutDialog);
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(aboutAction);

    QToolBar* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addAction(saveAction);

    // Matches Main.pas's btnViewIcons/btnViewList toolbar pair exactly
    // (mutually exclusive toggle buttons, not a single checkbox) - plan's
    // P2.4. See AppSettings.h's libraryThumbnailView() doc comment for why
    // this port's own default is Thumbnails, not the original's
    // ClassicListMode=true.
    toolbar->addSeparator();
    auto* viewModeGroup = new QActionGroup(this);
    viewModeGroup->setExclusive(true);
    viewThumbnailsAction_ = toolbar->addAction("Thumbnails");
    viewThumbnailsAction_->setObjectName("viewThumbnailsAction");
    viewThumbnailsAction_->setCheckable(true);
    viewThumbnailsAction_->setToolTip("Show a thumbnail preview for every library entry");
    viewModeGroup->addAction(viewThumbnailsAction_);
    viewListAction_ = toolbar->addAction("List");
    viewListAction_->setObjectName("viewListAction");
    viewListAction_->setCheckable(true);
    viewListAction_->setToolTip("Compact name-only list - skips thumbnail generation entirely, faster for "
                                 "large batches");
    viewModeGroup->addAction(viewListAction_);
    const bool startInThumbnailView = AppSettings::libraryThumbnailView();
    viewThumbnailsAction_->setChecked(startInThumbnailView);
    viewListAction_->setChecked(!startInThumbnailView);
    connect(viewThumbnailsAction_, &QAction::triggered, this, [this] { onViewModeChanged(true); });
    connect(viewListAction_, &QAction::triggered, this, [this] { onViewModeChanged(false); });
    applyListViewMode();

    statusBar()->showMessage("Ready");

    // onProgressTick()'s poll interval - matches RenderDialog's identical
    // timer (plan's P2.5).
    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(150);
    connect(progressTimer_, &QTimer::timeout, this, &MainWindow::onProgressTick);

    // RenderWorker lives on its own thread so a slow render never blocks
    // the UI event loop (see RenderWorker.h). Both are parented to `this`
    // for lifetime purposes but moveToThread() overrides *execution*
    // affinity, not the QObject parent/child ownership tree. Thumbnail
    // renders (see ThumbnailTask.h) go through QThreadPool instead - many
    // small renders in parallel, rather than this one dedicated thread's
    // one-at-a-time full-quality renders.
    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &MainWindow::renderRequested, worker_, &RenderWorker::renderFlameWithProgress);
    connect(worker_, &RenderWorker::renderFinished, this, &MainWindow::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();
}

MainWindow::~MainWindow() {
    workerThread_->quit();
    workerThread_->wait();
}

void MainWindow::openFlameFile(const QString& path) {
    statusBar()->showMessage("Loading " + path + "...");

    auto loaded = apo::loadFlameFile(path.toStdString());
    if (loaded.empty()) {
        QMessageBox::warning(this, "Open Flame", "Could not load any <flame> from:\n" + path);
        statusBar()->showMessage("Failed to load " + path);
        return;
    }

    ++loadGeneration_;
    flames_.clear();
    flames_.reserve(loaded.size());
    for (auto& f : loaded) {
        // A .flame saved after a final export often carries whatever huge
        // sampleDensity (and matching oversample/filter) that export used -
        // perfectly fine for a one-shot render, but every preview re-render
        // this app does while the flame just sits in the library or gets
        // edited (selection preview, camera-drag preview, EditorWindow's
        // live preview) uses that same real Flame::sampleDensity (see
        // requestRender()), so a file opened straight from a high-density
        // export made every one of those noticeably slow. Applying the same
        // reset New Flame/New Random Batch already get here too means an
        // opened file behaves like any other library flame: fast to look
        // at and edit, with density/oversample/etc. only cranked back up
        // deliberately in the Render dialog for the actual final export
        // (which reads the flame's *current* values at Render time, not
        // whatever the original file happened to have).
        applyDefaultRenderSettings(*f);
        flames_.push_back(std::shared_ptr<apo::Flame>(std::move(f)));
    }

    currentFilePath_ = path;
    setWindowTitle(QString("Apophysis 7X - %1 (%2 flame%3)")
                       .arg(path)
                       .arg(flames_.size())
                       .arg(flames_.size() == 1 ? "" : "s"));

    populateList();
    if (!flames_.empty()) flameList_->setCurrentRow(0);
}

void MainWindow::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void MainWindow::populateList() {
    flameList_->clear();
    const bool thumbnails = AppSettings::libraryThumbnailView();
    const QSize sizeHint = libraryItemSizeHint();
    for (size_t i = 0; i < flames_.size(); ++i) {
        const apo::Flame& f = *flames_[i];
        const QString label = f.name.empty() ? QString("Flame %1").arg(i + 1) : QString::fromStdString(f.name);
        auto* item = new QListWidgetItem(label);
        item->setSizeHint(sizeHint);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        flameList_->addItem(item);
        if (thumbnails) requestThumbnail(static_cast<int>(i));
    }
}

QSize MainWindow::libraryItemSizeHint() const {
    return AppSettings::libraryThumbnailView() ? QSize(kThumbnailSize + 20, kThumbnailSize + 34) : QSize();
}

void MainWindow::applyListViewMode() {
    const bool thumbnails = AppSettings::libraryThumbnailView();
    flameList_->setViewMode(thumbnails ? QListView::IconMode : QListView::ListMode);
    flameList_->setIconSize(thumbnails ? QSize(kThumbnailSize, kThumbnailSize) : QSize(0, 0));
    flameList_->setSpacing(thumbnails ? 6 : 2);
    const QSize sizeHint = libraryItemSizeHint();
    for (int i = 0; i < flameList_->count(); ++i) flameList_->item(i)->setSizeHint(sizeHint);
}

void MainWindow::onViewModeChanged(bool thumbnails) {
    AppSettings::setLibraryThumbnailView(thumbnails);
    applyListViewMode();
    if (thumbnails) {
        // Backfill: any item added while list mode was active never had a
        // thumbnail requested for it (see populateList()/
        // onDuplicateFlameTriggered()) - re-requesting for an item that
        // already has one is a little wasteful but harmless, and simpler
        // than tracking which items still need it.
        for (int i = 0; i < static_cast<int>(flames_.size()); ++i) requestThumbnail(i);
    }
}

void MainWindow::requestThumbnail(int index) {
    const int generation = loadGeneration_;
    auto* task = new ThumbnailTask(index, flames_[static_cast<size_t>(index)], kThumbnailSize);
    connect(task, &ThumbnailTask::finished, this, [this, generation](int i, QImage image) {
        if (generation != loadGeneration_) return; // stale - a different file has since been opened
        onThumbnailFinished(i, image);
    });
    QThreadPool::globalInstance()->start(task);
}

void MainWindow::onThumbnailFinished(int index, QImage image) {
    if (image.isNull() || index < 0 || index >= flameList_->count()) return;
    // setIcon() internally sets Qt::DecorationRole data, which - same as a
    // user's text edit - fires itemChanged; block it here so
    // onFlameItemChanged() only ever reacts to actual renames.
    flameList_->blockSignals(true);
    flameList_->item(index)->setIcon(QIcon(QPixmap::fromImage(image)));
    flameList_->blockSignals(false);
}

void MainWindow::onSelectionChanged(int row) {
    selectedIndex_ = row;
    updateLibraryActionsEnabled();
    if (row < 0 || row >= static_cast<int>(flames_.size())) {
        previewLabel_->setFlame(nullptr);
        return;
    }
    // PreviewLabel mutates this exact Flame instance directly on every
    // camera gesture (same "own the shared_ptr, edit it in place" contract
    // as TriangleCanvas::setFlame) - it's the same object flames_ and any
    // open EditorWindow for this flame both point at, so a zoom/pan here
    // is visible there too, matching how an Editor edit is visible here.
    previewLabel_->setFlame(flames_[static_cast<size_t>(row)]);
    startRender(flames_[static_cast<size_t>(row)]);
}

void MainWindow::onItemActivated(QListWidgetItem* item) { openFlameInEditor(flameList_->row(item)); }

void MainWindow::onEditFlameTriggered() { openFlameInEditor(selectedIndex_); }

void MainWindow::onDuplicateFlameTriggered() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;

    auto duplicate = flames_[static_cast<size_t>(selectedIndex_)]->clone();
    duplicate->name += " copy";

    const int insertAt = selectedIndex_ + 1;
    insertFlameAt(insertAt, std::shared_ptr<apo::Flame>(std::move(duplicate)));

    // Lands the selection on the duplicate, matching Add/Duplicate
    // Xform's own "select what you just created" convention in
    // EditorWindow. Not an undoable edit (A4's scope is camera
    // gesture/Reset Location/Delete/Rename/Paste - see onUndo's own doc
    // comment); Duplicate is trivially reversible via Delete already.
    flameList_->setCurrentRow(insertAt);
}

void MainWindow::onResetLocationTriggered() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;

    apo::Flame& flame = *flames_[static_cast<size_t>(selectedIndex_)];
    auto before = cloneShared(flame);
    flame.zoom = 0.0;
    apo::autoFrameFlame(flame, /*seed=*/1);
    pushUndo({UndoKind::Modify, selectedIndex_, before, cloneShared(flame)});
    startRender(flames_[static_cast<size_t>(selectedIndex_)]);
}

void MainWindow::openFlameInEditor(int row) {
    if (row < 0 || row >= static_cast<int>(flames_.size())) return;
    // EditorWindow gets its own independent shared_ptr to the SAME Flame
    // object the library list displays - editing it there is meant to be
    // visible back here too (matches the original: the Editor edits the
    // flame you opened it from, not a private copy). WA_DeleteOnClose (set
    // in EditorWindow's constructor) handles its lifetime; MainWindow
    // doesn't own or track open editor windows beyond this call.
    auto* editor = new EditorWindow(flames_[static_cast<size_t>(row)], this);
    editor->show();
}

void MainWindow::insertFlameAt(int index, std::shared_ptr<apo::Flame> flame) {
    // Every later index shifts up by one - invalidate any in-flight
    // thumbnail tasks so a stale completion callback can't write its
    // result into what's now a different flame's list item (see
    // loadGeneration_'s doc comment).
    ++loadGeneration_;
    flames_.insert(flames_.begin() + index, flame);

    const apo::Flame& inserted = *flames_[static_cast<size_t>(index)];
    const QString label =
        inserted.name.empty() ? QString("Flame %1").arg(index + 1) : QString::fromStdString(inserted.name);
    auto* item = new QListWidgetItem(label);
    item->setSizeHint(libraryItemSizeHint());
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    flameList_->insertItem(index, item);
    if (AppSettings::libraryThumbnailView()) requestThumbnail(index);
}

void MainWindow::removeFlameAt(int index) {
    // Every later index shifts down by one - same in-flight-thumbnail
    // invalidation insertFlameAt() needs, see its own comment.
    ++loadGeneration_;
    flames_.erase(flames_.begin() + index);
    delete flameList_->takeItem(index);
}

void MainWindow::selectRowAfterLibraryChange(int row) {
    if (flames_.empty()) {
        selectedIndex_ = -1;
        currentImage_ = QImage();
        previewLabel_->setFlame(nullptr);
        previewLabel_->setPixmap(QPixmap());
        statusBar()->showMessage("Library is empty");
    } else {
        selectedIndex_ = std::min(std::max(row, 0), static_cast<int>(flames_.size()) - 1);
        flameList_->blockSignals(true);
        flameList_->setCurrentRow(selectedIndex_);
        flameList_->blockSignals(false);
        previewLabel_->setFlame(flames_[static_cast<size_t>(selectedIndex_)]);
        startRender(flames_[static_cast<size_t>(selectedIndex_)]);
    }
    updateLibraryActionsEnabled();
}

void MainWindow::refreshListItemAt(int index) {
    if (index < 0 || index >= flameList_->count()) return;
    const apo::Flame& flame = *flames_[static_cast<size_t>(index)];
    const QString label =
        flame.name.empty() ? QString("Flame %1").arg(index + 1) : QString::fromStdString(flame.name);
    // Blocked so this doesn't loop back through onFlameItemChanged (which
    // would otherwise read this programmatic text change as a user rename
    // and push a second, redundant undo entry).
    flameList_->blockSignals(true);
    flameList_->item(index)->setText(label);
    flameList_->blockSignals(false);
    if (AppSettings::libraryThumbnailView()) requestThumbnail(index);
}

void MainWindow::pushUndo(UndoEntry entry) {
    undoStack_.push_back(std::move(entry));
    if (undoStack_.size() > kMaxUndoEntries) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
    updateUndoRedoActions();
}

void MainWindow::applyUndoEntry(const UndoEntry& entry, bool undoing) {
    switch (entry.kind) {
        case UndoKind::Modify: {
            if (entry.index < 0 || entry.index >= static_cast<int>(flames_.size())) break;
            const apo::Flame& source = undoing ? *entry.beforeFlame : *entry.afterFlame;
            flames_[static_cast<size_t>(entry.index)]->copyFrom(source);
            refreshListItemAt(entry.index);
            if (entry.index == selectedIndex_) startRender(flames_[static_cast<size_t>(entry.index)]);
            break;
        }
        case UndoKind::Insert: {
            // Paste inserted afterFlame at index - undoing removes it,
            // redoing re-inserts a fresh clone of it.
            if (undoing) {
                if (entry.index >= 0 && entry.index < static_cast<int>(flames_.size())) {
                    removeFlameAt(entry.index);
                    selectRowAfterLibraryChange(entry.index);
                }
            } else {
                insertFlameAt(entry.index, cloneShared(*entry.afterFlame));
                selectRowAfterLibraryChange(entry.index);
            }
            break;
        }
        case UndoKind::Delete: {
            // Delete removed beforeFlame from index - undoing re-inserts a
            // fresh clone of it, redoing removes it again.
            if (undoing) {
                insertFlameAt(entry.index, cloneShared(*entry.beforeFlame));
                selectRowAfterLibraryChange(entry.index);
            } else {
                if (entry.index >= 0 && entry.index < static_cast<int>(flames_.size())) {
                    removeFlameAt(entry.index);
                    selectRowAfterLibraryChange(entry.index);
                }
            }
            break;
        }
    }
}

void MainWindow::onUndo() {
    if (undoStack_.empty()) return;
    UndoEntry entry = std::move(undoStack_.back());
    undoStack_.pop_back();
    applyUndoEntry(entry, /*undoing=*/true);
    redoStack_.push_back(std::move(entry));
    updateUndoRedoActions();
}

void MainWindow::onRedo() {
    if (redoStack_.empty()) return;
    UndoEntry entry = std::move(redoStack_.back());
    redoStack_.pop_back();
    applyUndoEntry(entry, /*undoing=*/false);
    undoStack_.push_back(std::move(entry));
    updateUndoRedoActions();
}

void MainWindow::startRender(std::shared_ptr<apo::Flame> flame) { requestRender(flame, /*cameraPreview=*/false); }

void MainWindow::requestRender(std::shared_ptr<apo::Flame> flame, bool cameraPreview) {
    if (renderInFlight_) {
        renderDirty_ = true;
        pendingCameraPreview_ = cameraPreview;
        return;
    }
    renderInFlight_ = true;
    statusBar()->showMessage("Rendering...");

    if (!cameraPreview) {
        // Plan's P2.5 - only the full-quality path gets a progress token;
        // see MainWindow.h's renderRequested doc comment for why the
        // cheap reduced-quality camera-preview path deliberately doesn't.
        progress_ = std::make_unique<apo::RenderProgress>();
        elapsedTimer_.start();
        progressTimer_->start();
        emit renderRequested(flame, /*seed=*/1, progress_.get());
        return;
    }

    progress_.reset();
    progressTimer_->stop();

    // Reduced-quality preview clone - see requestCameraPreviewRender()'s
    // doc comment. Fits the clone to previewLabel_'s own available space
    // while preserving the flame's own declared aspect ratio (rather than
    // just using the label's raw width/height, which would visibly
    // squish/stretch the preview whenever the label and flame don't share
    // an aspect ratio - a jarring artifact this port doesn't need to
    // accept just because EditorWindow's own preview, which fills a
    // dedicated square-ish canvas widget, gets away with not bothering).
    auto previewFlame = flame->clone();
    int pw = std::max(previewLabel_->width(), 64);
    int ph = std::max(previewLabel_->height(), 64);
    const double aspect = flame->height > 0 ? static_cast<double>(flame->width) / flame->height : 1.0;
    if (static_cast<double>(pw) / ph > aspect) {
        pw = std::max(64, static_cast<int>(ph * aspect));
    } else {
        ph = std::max(64, static_cast<int>(pw / aspect));
    }
    previewFlame->adjustScale(pw, ph);
    previewFlame->sampleDensity = AppSettings::previewSampleDensity();

    std::shared_ptr<const apo::Flame> shared(std::move(previewFlame));
    emit renderRequested(shared, /*seed=*/1, nullptr);
}

void MainWindow::requestCameraPreviewRender() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;
    requestRender(flames_[static_cast<size_t>(selectedIndex_)], /*cameraPreview=*/true);
}

void MainWindow::onCameraGestureStarted() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;
    hasPendingCameraUndo_ = true;
    pendingCameraUndoIndex_ = selectedIndex_;
    pendingCameraUndoBefore_ = cloneShared(*flames_[static_cast<size_t>(selectedIndex_)]);
}

void MainWindow::onCameraChanged() { requestCameraPreviewRender(); }

void MainWindow::onCameraChangeFinished() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;

    if (hasPendingCameraUndo_ && pendingCameraUndoIndex_ == selectedIndex_) {
        pushUndo({UndoKind::Modify, selectedIndex_, pendingCameraUndoBefore_,
                  cloneShared(*flames_[static_cast<size_t>(selectedIndex_)])});
    }
    hasPendingCameraUndo_ = false;
    pendingCameraUndoBefore_.reset();

    startRender(flames_[static_cast<size_t>(selectedIndex_)]);
}

void MainWindow::onProgressTick() {
    if (!progress_) return;

    const std::uint64_t done = progress_->pointsDone.load(std::memory_order_relaxed);
    const std::uint64_t target = progress_->pointsTarget.load(std::memory_order_relaxed);
    const double prog = target > 0 ? static_cast<double>(done) / static_cast<double>(target) : 0.0;
    const int percent = static_cast<int>(std::min<std::uint64_t>(100, target > 0 ? done * 100 / target : 0));

    const double elapsedSec = elapsedTimer_.elapsed() / 1000.0;
    // Matches Main.pas's OnProgress exactly: remaining = elapsed/progress -
    // elapsed (extrapolate a total time from progress-so-far, subtract
    // what's already elapsed) - verified directly against source, not
    // guessed.
    const double remainingSec = prog > 0.0 ? (elapsedSec / prog - elapsedSec) : 0.0;

    statusBar()->showMessage(QString("Rendering... %1% (%2s elapsed, ~%3s remaining)")
                                  .arg(percent)
                                  .arg(elapsedSec, 0, 'f', 1)
                                  .arg(remainingSec, 0, 'f', 1));
}

void MainWindow::onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted) {
    progressTimer_->stop();
    progress_.reset();

    currentImage_ = image;
    previewLabel_->setRenderedImageSize(image.size());
    updatePreviewScaling();

    const double acceptedPct =
        pointsGenerated > 0 ? 100.0 * static_cast<double>(pointsAccepted) / static_cast<double>(pointsGenerated) : 0.0;
    statusBar()->showMessage(QString("Rendered %1x%2 - %3 points, %4% accepted")
                                  .arg(image.width())
                                  .arg(image.height())
                                  .arg(pointsGenerated)
                                  .arg(acceptedPct, 0, 'f', 1));

    renderInFlight_ = false;
    if (renderDirty_) {
        renderDirty_ = false;
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(flames_.size())) {
            requestRender(flames_[static_cast<size_t>(selectedIndex_)], pendingCameraPreview_);
        }
        return;
    }

    if (!autoScreenshotPath_.isEmpty()) {
        // Deferred to the next event-loop iteration so the label has
        // actually repainted with the new pixmap before it's grabbed -
        // grab() captures whatever's currently on screen, and the paint
        // triggered by setPixmap() above hasn't necessarily happened yet
        // at this exact point in onRenderFinished.
        const QString path = autoScreenshotPath_;
        const bool exitAfter = autoScreenshotExit_;
        autoScreenshotPath_.clear();
        QTimer::singleShot(100, this, [this, path, exitAfter] {
            grab().save(path, "PNG");
            if (exitAfter) close();
        });
    }
}

void MainWindow::onOpenTriggered() {
    const QString path =
        QFileDialog::getOpenFileName(this, "Open Flame", QString(), "Flame files (*.flame *.flam3);;All files (*)");
    if (!path.isEmpty()) openFlameFile(path);
}

void MainWindow::onSaveRenderTriggered() {
    if (currentImage_.isNull()) {
        QMessageBox::information(this, "Save Render", "Nothing has been rendered yet.");
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, "Save Render As", QString(), "PNG image (*.png)");
    if (path.isEmpty()) return;

    if (!currentImage_.save(path, "PNG")) {
        QMessageBox::warning(this, "Save Render", "Failed to write:\n" + path);
    } else {
        statusBar()->showMessage("Saved " + path);
    }
}

void MainWindow::onNewFlameTriggered() {
    // Not added to flames_/the library list - a blank flame doesn't come
    // from (or belong to) any loaded file, matching how EditorWindow
    // already treats every flame it edits as independent of MainWindow's
    // own list once opened.
    auto* editor = new EditorWindow(makeBlankFlame(), this);
    editor->show();
}

void MainWindow::onNewRandomBatchTriggered() {
    bool ok = false;
    const int count = QInputDialog::getInt(this, "New Random Batch", "Number of flames:",
                                            /*value=*/AppSettings::randomBatchSize(), /*min=*/1,
                                            /*max=*/300, /*step=*/1, &ok);
    if (!ok) return;

    statusBar()->showMessage(QString("Generating %1 random flames...").arg(count));

    // Captured before flames_ is cleared - Main.pas's own "Keep background
    // color" option preserves whatever background the *current* flame
    // already had rather than resetting every new one to black; this
    // port's batch replaces the whole library at once (see below), so
    // "current" is whichever flame was selected right before generating
    // the new batch.
    std::array<int, 4> keepBackgroundColor{0, 0, 0, 0};
    // Also doubles as the "Current gradient" gradient-source option's
    // source (Options.pas's "On random flame use..." - see AppSettings.h's
    // randomGradientSource() doc comment): both features mean "whatever
    // the currently-selected library flame already had", captured here for
    // the same reason keepBackgroundColor is - flames_ is about to be
    // cleared below.
    apo::ColorMap currentGradient;
    const bool hasCurrentGradient = selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(flames_.size());
    if (hasCurrentGradient) currentGradient = flames_[static_cast<size_t>(selectedIndex_)]->cmap;
    if (AppSettings::randomKeepBackground() && hasCurrentGradient) {
        keepBackgroundColor = flames_[static_cast<size_t>(selectedIndex_)]->background;
    }

    // Matches Main.pas's RandomBatch: replaces the current library view
    // with a freshly-generated batch, not appended alongside whatever was
    // already loaded (see openFlameFile()'s identical flames_.clear()).
    ++loadGeneration_;
    flames_.clear();
    flames_.reserve(static_cast<size_t>(count));

    const int minXforms = AppSettings::randomMinXforms();
    const int maxXforms = AppSettings::randomMaxXforms();
    const QString titlePrefix = AppSettings::randomBatchTitlePrefix();
    // 0=None, 1=Bilateral, 2=Rotational, 3=Dihedral - matches Options.dfm's
    // cmbSymType.Items order exactly (see AppSettings.h).
    const int symType = AppSettings::randomSymmetryType();
    const int symOrder = AppSettings::randomSymmetryOrder();

    // Options.pas's "Variation" menu equivalent (Options > Random >
    // Variation) - empty/not-found both mean "Random" (variationIndex -1).
    const QString forcedVariationName = AppSettings::randomForcedVariationName();
    int forcedVariationIndex = -1;
    if (!forcedVariationName.isEmpty()) {
        forcedVariationIndex = apo::VariationRegistry::instance().variationIndex(forcedVariationName.toStdString());
    }
    // Options.pas's Variations tab checklist - restricts the "Random"
    // draw above (a forced variation, if any, bypasses this entirely,
    // matching randomizeXforms's own variationIndex>=0 short-circuit).
    const std::vector<int> eligibleVariations = AppSettings::enabledVariationIndices();
    const auto gradientSource = static_cast<apo::RandomGradientSource>(std::clamp(AppSettings::randomGradientSource(), 0, 2));

    // Options.pas has no equivalent of this group - a new setting this port
    // adds (see AppSettings.h's own comment on why it defaults to "on"
    // rather than the old single-variation-only behavior).
    const int minVariationsPerXform = AppSettings::randomMinVariationsPerXform();
    const int maxVariationsPerXform = AppSettings::randomMaxVariationsPerXform();
    const double variationWeightMin = AppSettings::randomVariationWeightMin();
    const double variationWeightMax = AppSettings::randomVariationWeightMax();
    const bool randomizeVariationParameters = AppSettings::randomizeVariationParameters();
    const double variationParameterStrength = AppSettings::randomVariationParameterStrength();

    // Distinct, splitmix64-mixed seeds per flame from one random base -
    // same derivation Renderer.cpp already uses for its own per-thread
    // seeds, reused here so each flame in the batch is independently
    // well-seeded rather than just base+i (see Renderer.cpp's own comment
    // on why adjacent integer seeds can correlate for some PRNGs).
    const auto baseSeed = static_cast<std::uint64_t>(std::random_device{}());
    for (int i = 0; i < count; ++i) {
        const std::uint64_t seed = baseSeed + static_cast<std::uint64_t>(i) * 0x9e3779b97f4a7c15ULL;
        auto flame = apo::generateRandomFlame(seed, /*width=*/640, /*height=*/480, minXforms, maxXforms,
                                               forcedVariationIndex, &eligibleVariations, gradientSource,
                                               hasCurrentGradient ? &currentGradient : nullptr, minVariationsPerXform,
                                               maxVariationsPerXform, variationWeightMin, variationWeightMax,
                                               randomizeVariationParameters, variationParameterStrength);
        flame->name = QString("%1 %2").arg(titlePrefix).arg(i + 1).toStdString();
        applyDefaultRenderSettings(*flame);

        // Forced symmetry on random generation (Options.pas's Random tab) -
        // matches Main.pas's own sign convention exactly (see
        // core/edit/Symmetry.h's doc comment): Bilateral -> sym=-1,
        // Rotational -> sym=+Order, Dihedral -> sym=-Order.
        switch (symType) {
            case 1: apo::addSymmetry(*flame, -1); break;
            case 2: apo::addSymmetry(*flame, symOrder); break;
            case 3: apo::addSymmetry(*flame, -symOrder); break;
            default: break; // 0 = None
        }

        if (AppSettings::randomKeepBackground()) flame->background = keepBackgroundColor;

        flames_.push_back(std::shared_ptr<apo::Flame>(std::move(flame)));
    }

    currentFilePath_.clear();
    setWindowTitle(QString("Apophysis 7X - Random Batch (%1 flame%2)").arg(count).arg(count == 1 ? "" : "s"));

    populateList();
    if (!flames_.empty()) flameList_->setCurrentRow(0);
}

void MainWindow::onSaveFlameAsTriggered() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) {
        QMessageBox::information(this, "Save Flame", "Select a flame in the library first.");
        return;
    }
    const apo::Flame& flame = *flames_[static_cast<size_t>(selectedIndex_)];
    const QString suggested = flame.name.empty() ? "untitled.flame" : QString::fromStdString(flame.name) + ".flame";
    const QString path = QFileDialog::getSaveFileName(this, "Save Flame As", suggested, "Flame files (*.flame)");
    if (path.isEmpty()) return;

    if (!apo::saveFlameFile(path.toStdString(), {&flame})) {
        QMessageBox::warning(this, "Save Flame", "Failed to write:\n" + path);
    } else {
        statusBar()->showMessage("Saved " + path);
    }
}

void MainWindow::onSaveAllFlamesTriggered() {
    if (flames_.empty()) {
        QMessageBox::information(this, "Save All Parameters", "The library is empty - nothing to save.");
        return;
    }
    const QString suggested = currentFilePath_.isEmpty() ? "untitled.flame" : currentFilePath_;
    const QString path = QFileDialog::getSaveFileName(this, "Save All Parameters", suggested, "Flame files (*.flame)");
    if (path.isEmpty()) return;

    std::vector<const apo::Flame*> allFlames;
    allFlames.reserve(flames_.size());
    for (const auto& flame : flames_) allFlames.push_back(flame.get());

    if (!apo::saveFlameFile(path.toStdString(), allFlames)) {
        QMessageBox::warning(this, "Save All Parameters", "Failed to write:\n" + path);
    } else {
        statusBar()->showMessage(QString("Saved %1 flame%2 to %3").arg(flames_.size()).arg(flames_.size() == 1 ? "" : "s").arg(path));
    }
}

void MainWindow::onRenderAllFlamesTriggered() {
    if (flames_.empty()) {
        QMessageBox::information(this, "Render All Flames", "The library is empty - nothing to render.");
        return;
    }
    // RenderAllDialog takes its own independent copy of the flames_ vector
    // (shared_ptrs, not the Flame objects themselves) - it renders clones
    // of each (see RenderAllDialog::renderNext()), never touching the
    // library MainWindow displays.
    auto* dialog = new RenderAllDialog(flames_, this);
    dialog->show();
}

void MainWindow::onSmoothPaletteTriggered() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;

    auto* dialog = new SmoothPaletteDialog(this);
    const int targetIndex = selectedIndex_; // captured now - the dialog is non-modal, selection may move on before Apply
    connect(dialog, &SmoothPaletteDialog::paletteApplied, this, [this, targetIndex](apo::ColorMap cmap) {
        if (targetIndex < 0 || targetIndex >= static_cast<int>(flames_.size())) return; // the library changed since Smooth Palette was opened
        apo::Flame& flame = *flames_[static_cast<size_t>(targetIndex)];
        auto before = cloneShared(flame);
        flame.cmap = cmap;
        pushUndo({UndoKind::Modify, targetIndex, before, cloneShared(flame)});
        if (targetIndex == selectedIndex_) startRender(flames_[static_cast<size_t>(targetIndex)]);
    });
    dialog->show();
}

void MainWindow::onCopyFlameTriggered() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;
    const apo::Flame& flame = *flames_[static_cast<size_t>(selectedIndex_)];
    const QString xml = QString::fromStdString(apo::saveFlameToString({&flame}));
    QGuiApplication::clipboard()->setText(xml);
    statusBar()->showMessage("Copied " + QString::fromStdString(flame.name) + " to the clipboard");
}

void MainWindow::onPasteFlameTriggered() {
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty()) {
        QMessageBox::information(this, "Paste Flame", "The clipboard is empty.");
        return;
    }

    auto parsed = apo::loadFlameFromString(text.toStdString());
    if (parsed.empty()) {
        QMessageBox::warning(this, "Paste Flame", "The clipboard does not contain a valid flame.");
        return;
    }

    // Matches the original's own single-flame Paste (mnuPasteClick parses
    // into MainCp, never a whole library) - only the first <flame> in the
    // clipboard text is inserted, even if it's a multi-flame <flames>
    // document (e.g. an entire library copied from elsewhere).
    std::shared_ptr<apo::Flame> pasted(std::move(parsed.front()));

    // Same "insert after the current selection, select the new entry"
    // shape as onDuplicateFlameTriggered - selectedIndex_ == -1 (nothing
    // selected, including an empty library) lands insertAt at 0.
    const int insertAt = selectedIndex_ + 1;
    insertFlameAt(insertAt, pasted);
    pushUndo({UndoKind::Insert, insertAt, nullptr, cloneShared(*pasted)});

    const QString label = flameList_->item(insertAt)->text();
    flameList_->setCurrentRow(insertAt);
    statusBar()->showMessage("Pasted " + label + " from the clipboard");
}

void MainWindow::onRenameFlameTriggered() {
    if (selectedIndex_ < 0 || selectedIndex_ >= flameList_->count()) return;
    flameList_->editItem(flameList_->item(selectedIndex_));
}

void MainWindow::onFlameItemChanged(QListWidgetItem* item) {
    const int row = flameList_->row(item);
    if (row < 0 || row >= static_cast<int>(flames_.size())) return;

    apo::Flame& flame = *flames_[static_cast<size_t>(row)];
    const std::string newName = item->text().toStdString();
    if (newName == flame.name) return; // no-op edit (e.g. re-confirmed unchanged)

    auto before = cloneShared(flame);
    flame.name = newName;
    pushUndo({UndoKind::Modify, row, before, cloneShared(flame)});
}

void MainWindow::onDeleteFlameTriggered() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(flames_.size())) return;

    const apo::Flame& target = *flames_[static_cast<size_t>(selectedIndex_)];
    const QString name =
        target.name.empty() ? QString("Flame %1").arg(selectedIndex_ + 1) : QString::fromStdString(target.name);
    const auto reply =
        QMessageBox::question(this, "Delete Flame",
                               QString("Remove \"%1\" from this library?\n\nThis only removes it from the "
                                       "current session's library - it does not modify any file on disk.")
                                   .arg(name),
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    const int row = selectedIndex_;
    pushUndo({UndoKind::Delete, row, cloneShared(target), nullptr});
    removeFlameAt(row);
    selectRowAfterLibraryChange(row);
}

void MainWindow::onFlameListContextMenuRequested(const QPoint& pos) {
    QListWidgetItem* item = flameList_->itemAt(pos);
    if (!item) return;
    flameList_->setCurrentItem(item); // right-clicking an unselected item selects it first, standard list UX

    QMenu menu(this);
    menu.addAction(editFlameAction_);
    menu.addAction(duplicateFlameAction_);
    menu.addAction(renameFlameAction_);
    menu.addAction(deleteFlameAction_);
    menu.addSeparator();
    menu.addAction(resetLocationAction_);
    menu.exec(flameList_->mapToGlobal(pos));
}

void MainWindow::updateLibraryActionsEnabled() {
    const bool hasSelection = selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(flames_.size());
    copyFlameAction_->setEnabled(hasSelection);
    editFlameAction_->setEnabled(hasSelection);
    duplicateFlameAction_->setEnabled(hasSelection);
    renameFlameAction_->setEnabled(hasSelection);
    deleteFlameAction_->setEnabled(hasSelection);
    resetLocationAction_->setEnabled(hasSelection);
    smoothPaletteAction_->setEnabled(hasSelection);
}

void MainWindow::updateUndoRedoActions() {
    undoAction_->setEnabled(!undoStack_.empty());
    redoAction_->setEnabled(!redoStack_.empty());
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updatePreviewScaling();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    // Accepts any drag carrying at least one local file - not gated on a
    // .flame/.flam3 extension, matching openFlameFile()'s own
    // extension-agnostic behavior (loadFlameFile just tries to parse
    // whatever's there as XML and fails gracefully with a warning box, the
    // same as picking an unrelated file via File > Open's "All files"
    // filter would).
    const QMimeData* mime = event->mimeData();
    if (mime && mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (url.isLocalFile()) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    if (!mime || !mime->hasUrls()) {
        event->ignore();
        return;
    }
    // Only the first local file - openFlameFile()'s own "replaces the
    // whole library" contract (matches every Main.pas list-populating path
    // - see MainWindow.h's own class comment) already means a second
    // dropped file couldn't add to the first's library anyway, the same
    // constraint File > Open (a single-file picker) already has.
    for (const QUrl& url : mime->urls()) {
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            openFlameFile(url.toLocalFile());
            return;
        }
    }
    event->ignore();
}

void MainWindow::updatePreviewScaling() {
    if (currentImage_.isNull()) return;
    previewLabel_->setPixmap(
        QPixmap::fromImage(currentImage_).scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::openOptionsDialog() {
    OptionsDialog dialog(this);
    dialog.exec();
}

void MainWindow::openAboutDialog() {
    AboutDialog dialog(this);
    dialog.exec();
}

} // namespace apo::ui
