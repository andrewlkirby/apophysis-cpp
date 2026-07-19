#pragma once

#include <memory>
#include <vector>

#include <QElapsedTimer>
#include <QImage>
#include <QMainWindow>
#include <QPoint>
#include <QString>

#include "core/Flame.h"
#include "core/render/Renderer.h"

class QListWidget;
class QListWidgetItem;
class QThread;
class QAction;
class QTimer;
class QDragEnterEvent;
class QDropEvent;

namespace apo::ui {

class RenderWorker;
class PreviewLabel;

// Phase 6's Main window: a flame library (list + thumbnails, replacing
// Main.pas's owner-drawn flame list) on the left, a live preview of
// whichever flame is selected on the right - the open-render-display core
// loop everything else (the Editor's live preview, the Adjust dialog's
// re-render-on-change) builds on.
//
// Multi-file accumulation and drag-reordering were both listed as gaps in
// an earlier status pass, but neither turns out to be real: every one of
// Main.pas's list-populating paths (ListXmlSimple/ListXmlThumbnails/
// TThumbnailThread.Create) calls `ListView1.Items.Clear` before
// repopulating - opening a file always *replaces* the library there too -
// and there is no drag-drop/reorder code anywhere in Main.pas/Main.dfm.
// What *is* real and was missing: renaming a library entry
// (mnuListRenameClick -> TListItem.EditCaption -> RenameXML) and removing
// one (mnuItemDeleteClick, confirm-gated). Both are implemented below
// (onRenameFlameTriggered/onDeleteFlameTriggered), but deliberately not as
// a port of RenameXML's own mechanism - that function rewrites the
// *source .flame file on disk* immediately via raw text search-and-replace
// (with a fragile failure mode: a plain StringReplace on the whole line,
// which would corrupt the file if the old name happened to appear as a
// substring elsewhere on that same line) the moment a rename is committed,
// with no explicit save step. This port's whole persistence model is
// explicit-save-only (nothing else silently writes to disk) - Rename/
// Delete here only ever touch the in-memory flames_ list; a renamed or
// pruned library still needs an explicit save (Save Flame As for the
// selected flame, or Save All Parameters for the whole library - see
// onSaveAllFlamesTriggered()) to persist anywhere.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Loads every <flame> in `path` into the library list and selects the
    // first one (which triggers its full-quality render).
    void openFlameFile(const QString& path);

    // Testing/verification hook, not a normal user-facing feature: once
    // the current render completes, grabs the preview and saves it to
    // `path`, then (if `exitAfter`) quits the application. This is what
    // lets a render regression be caught by *actually launching the GUI
    // and looking at its output*, the same spirit as
    // baseline_regression_test.cpp but exercising the real widget/paint
    // path rather than just the core render library.
    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void resizeEvent(QResizeEvent* event) override;
    // A7 - standard desktop drag-and-drop: dropping a .flame/.flam3 (or any
    // file loadFlameFile can parse - see openFlameFile()'s own graceful
    // failure handling) onto the window opens it, same as File > Open.
    // Not a port (grep confirms Main.pas/Main.dfm has no drag-drop
    // handling at all) - just standard, expected desktop behavior a modern
    // Qt app gets for ~20 lines.
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

signals:
    // `progress` is non-null only for the full-quality preview render a
    // fresh selection or a settled camera gesture triggers (plan's P2.5) -
    // null for the cheap reduced-quality renders a continuous camera drag
    // fires on every mouse-move, which are fast enough that a progress
    // readout would just flicker uselessly. Owned by this window
    // (progress_ below), not the receiver - see RenderWorker::renderFull's
    // identical ownership contract for the same kind of pointer.
    void renderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed, apo::RenderProgress* progress);

private slots:
    void onOpenTriggered();
    void onSaveRenderTriggered();
    void onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted);
    void onSelectionChanged(int row);
    void onThumbnailFinished(int index, QImage image);
    // Fires on double-click *or* pressing Enter/Return with an item
    // current (Qt's own QAbstractItemView::activated semantics on Windows
    // style) - covers both without risking a double-open on an actual
    // double-click, since only one signal (itemActivated) is connected to
    // this, not two.
    void onItemActivated(QListWidgetItem* item);
    void openOptionsDialog();
    void openAboutDialog();
    void onNewFlameTriggered();
    void onSaveFlameAsTriggered();
    // Matches Main.dfm's mnuSaveAllAs (File > Save All Parameters): writes
    // every flame currently in the library to one .flame file, not just the
    // selected one - see this class's own doc comment on why Rename/Delete
    // otherwise have no way to actually persist a curated library.
    void onSaveAllFlamesTriggered();
    // Matches Main.dfm's mnuCopy/mnuPaste (Edit menu, Ctrl+C/Ctrl+V) - the
    // system-clipboard flame-XML workflow most shared flames actually
    // arrive/leave through (forum posts, Discord, ...), distinct from
    // EditorWindow's own in-app xform-only clipboard snapshot. Unlike the
    // original (whose Paste overwrites the single currently-open MainCp in
    // place), this port's library is a list - Paste inserts a new entry
    // (same "insert after the current selection, then select it" shape as
    // onDuplicateFlameTriggered) rather than silently mutating whatever's
    // selected.
    void onCopyFlameTriggered();
    void onPasteFlameTriggered();
    // A4's main-window undo/redo (Ctrl+Z/Ctrl+Shift+Z) - a bounded
    // (kMaxUndoEntries) stack of whole-Flame before/after snapshots
    // (UndoEntry below), deliberately NOT a port of the original's
    // whole-app undo web (Edit > Undo/Redo, mnuUndo/mnuRedo): scoped to
    // the five MainWindow-level edits that were previously irreversible -
    // camera gesture settle, Reset Location, Delete, Rename, Paste (A2) -
    // the same reduced scope EditorWindow's own undo takes for its own
    // domain (xform edits only, not e.g. gradient changes).
    void onUndo();
    void onRedo();
    // Matches Main.dfm's mnuRenderAll (Flame > "Render All Flames") - opens
    // RenderAllDialog.h over the whole library, the batch-render
    // counterpart to onSaveAllFlamesTriggered() above.
    void onRenderAllFlamesTriggered();
    // A5 - matches Main.dfm's mnuSmoothGradient (File > "Smooth
    // Palette..."). Opens SmoothPaletteDialog.h non-modally over whichever
    // flame is selected *right now*; unlike RenderDialog/RenderAllDialog
    // (which each clone their target once at construction), the dialog
    // itself holds no Flame at all (same "no target, just emit the result"
    // shape as GradientBrowserDialog) - the target index is captured in
    // this method's own lambda instead, applied as a Modify undo entry
    // (same shape as Reset Location) when the dialog's paletteApplied
    // signal actually fires, which may be much later while the dialog sits
    // open.
    void onSmoothPaletteTriggered();
    void onNewRandomBatchTriggered();
    void onEditFlameTriggered();
    void onDuplicateFlameTriggered();
    void onRenameFlameTriggered();
    void onDeleteFlameTriggered();
    // Matches Main.pas's ResetLocation (Flame menu, F12 - confirmed via
    // ShortCut=123 in Main.dfm): zeroes the selected flame's zoom, then
    // re-frames its center/pixelsPerUnit around its own actual content via
    // AutoFrame.h's autoFrameFlame() (this port's non-iterative equivalent
    // of the original's CalcBoundbox - see that header's own doc comment).
    // Notably NOT a hard reset of center to (0,0): the original's own
    // ResetLocation computes a fresh CalcBoundbox-derived center instead
    // (its FAngle reset is commented out in the source - dead in the
    // original itself - so this port doesn't carry it either).
    void onResetLocationTriggered();
    void onFlameListContextMenuRequested(const QPoint& pos);
    void onFlameItemChanged(QListWidgetItem* item);
    // PreviewLabel's camera-gesture signals (plan's P2.1: mouse-wheel zoom
    // + drag-pan directly on the preview, writing into the selected
    // flame's own Flame::zoom/center) - cameraChanged() during continuous
    // dragging asks for a cheap preview-quality re-render, matching every
    // other live-interactive preview in this port (Editor/Adjust/Mutate/
    // Curves/PostProcess all share AppSettings::previewSampleDensity() for
    // exactly this reason); cameraChangeFinished() (a drag's mouse
    // release, or immediately after one wheel-zoom step, which is already
    // a complete gesture) asks for a full-quality settle-render instead,
    // reusing the exact same path a fresh selection already renders
    // through.
    // PreviewLabel::gestureStarted() - fires before any mutation, the only
    // correct point to snapshot a Modify undo entry's "before" state (see
    // hasPendingCameraUndo_'s own doc comment: cameraChanged() itself
    // already fires *after* that step's mutation, too late).
    void onCameraGestureStarted();
    void onCameraChanged();
    void onCameraChangeFinished();
    // Polls progress_ (~150ms, matching RenderDialog's identical timer)
    // while a full-quality preview render is in flight, showing elapsed/
    // remaining time and percent complete in the status bar - matches
    // Main.pas's OnProgress (plan's P2.5: "elapsed/remaining time in the
    // status bar"), including its own elapsed/(remaining = elapsed/
    // progress - elapsed) formula, verified directly against source
    // rather than guessed. Not a port of the original's *progressively
    // refining bitmap* (the other half of "progressive main preview") -
    // that comes from the original's own incremental-bucket render
    // architecture, which this port's Renderer deliberately does not
    // replicate (see Renderer.h's class comment on the one-shot, no-
    // intermediate-buffer design chosen for performance) - a real
    // architectural difference, not a missing nicety.
    void onProgressTick();
    // Matches Main.pas's btnViewIcons/btnViewList (plan's P2.4) - toggles
    // the library list between thumbnail/icon view and a compact name-
    // only list. `thumbnails` false also stops generating thumbnails at
    // all for anything added afterward (matches the original's own real
    // performance reason for the feature, not just a layout change - see
    // AppSettings.h's libraryThumbnailView() doc comment); switching back
    // to true backfills thumbnails for every currently-loaded flame that
    // never got one requested while list mode was active.
    void onViewModeChanged(bool thumbnails);

private:
    void startRender(std::shared_ptr<apo::Flame> flame);
    // Shared coalescing gate behind both startRender() (full quality) and
    // requestCameraPreviewRender() (reduced quality, see below) - without
    // it, a continuous drag gesture would queue a full render per mouse-
    // move on RenderWorker's single-render-at-a-time thread, the exact
    // "unusably slow" failure mode EditorWindow::requestRender()'s own
    // doc comment already flags for the Editor's triangle drags. At most
    // one render is ever in flight; a request arriving while one is
    // already running is coalesced (renderDirty_) and re-issued - always
    // against whatever is selected *now*, not a stale captured pointer -
    // once the in-flight one completes (see onRenderFinished()).
    void requestRender(std::shared_ptr<apo::Flame> flame, bool cameraPreview);
    // Builds a scaled-down, reduced-sampleDensity clone of the currently
    // selected flame (same "clone, adjustScale, override sampleDensity"
    // shape as EditorWindow::requestRender() - see its own doc comment for
    // why rendering the flame as-is on every mouse-move would be a real
    // performance problem, not just a missing nicety) and requests it via
    // requestRender(..., cameraPreview=true).
    void requestCameraPreviewRender();
    void updatePreviewScaling();
    void populateList();
    void requestThumbnail(int index);
    void updateLibraryActionsEnabled();
    // Enables undoAction_/redoAction_ from undoStack_/redoStack_'s own
    // emptiness - called after every push/undo/redo.
    void updateUndoRedoActions();
    // Applies AppSettings::libraryThumbnailView()'s current value to
    // flameList_'s own view-mode/icon-size/spacing and every existing
    // item's size hint - called once at startup and again whenever the
    // toggle changes, so populateList()/onDuplicateFlameTriggered() (new
    // items) and onViewModeChanged() (existing items) share one place
    // that knows what "the current mode" actually looks like.
    void applyListViewMode();
    // QSize(kThumbnailSize+20, kThumbnailSize+34) in thumbnail view, or a
    // default-constructed QSize() (let the view compute a natural compact
    // size from the item's own content) in list view - what a *newly
    // added* library item's size hint should be right now.
    QSize libraryItemSizeHint() const;
    // Shared by onItemActivated (double-click/Enter) and
    // onEditFlameTriggered (the explicit Library-menu/context-menu
    // action) - both open the same flame the same way, so a batch-
    // generated flame is exactly as editable as a file-opened one.
    void openFlameInEditor(int row);

    // Inserts `flame` into flames_ at `index`, creating the matching list
    // item (requesting a thumbnail if thumbnail view is active) - the
    // list-mutation half of onDuplicateFlameTriggered/onPasteFlameTriggered
    // and of undo/redo's own Insert/Delete handling (applyUndoEntry).
    // Never touches selection - every caller decides that for itself.
    void insertFlameAt(int index, std::shared_ptr<apo::Flame> flame);
    // Removes flames_[index] and its list item - the list-mutation half of
    // onDeleteFlameTriggered and of undo/redo's own Insert/Delete handling.
    // Never touches selection - every caller decides that for itself.
    void removeFlameAt(int index);
    // Selects `row` and drives every side effect onSelectionChanged(row)
    // would - directly, rather than relying on
    // QListWidget::currentRowChanged, which does NOT fire if `row`'s
    // numeric value happens to already be the current row even though the
    // underlying Flame at that index just changed (e.g. undoing a Delete
    // re-inserts a flame at the same row a different one now occupies) -
    // see onDeleteFlameTriggered's own pre-A4 comment on this exact
    // failure mode, now shared here since undo/redo hits it too.
    void selectRowAfterLibraryChange(int row);
    // Refreshes list item `index`'s text (from flames_[index]->name) and
    // thumbnail - shared by undo/redo of a Modify entry (Rename changes
    // the name; Reset Location/a camera gesture change the rendered
    // appearance a thumbnail should reflect).
    void refreshListItemAt(int index);

    // One undo-history entry - three kinds (unlike EditorWindow's two,
    // Drag/Structural) because MainWindow-level edits fall into three
    // distinct shapes:
    //   - Modify: flames_[index] itself is mutated in place (Rename, Reset
    //     Location, a settled camera gesture) - beforeFlame/afterFlame are
    //     whole-flame clones of its state right before/after the edit.
    //     Applied via Flame::copyFrom (not swapping the shared_ptr), so
    //     PreviewLabel/an open EditorWindow - both of which hold the exact
    //     same shared_ptr<Flame> flames_[index] does - see the undo too,
    //     the same reason MutateDialog adopting a mutant uses copyFrom
    //     instead of clone() (see Flame.h's own doc comment).
    //   - Insert: a flame was inserted at index (Paste) - afterFlame is a
    //     clone of what was inserted (beforeFlame unused); undoing removes
    //     it, redoing re-inserts a fresh clone of afterFlame.
    //   - Delete: a flame was removed from index - beforeFlame is a clone
    //     of what was removed (afterFlame unused); undoing re-inserts a
    //     fresh clone of beforeFlame, redoing removes it again.
    enum class UndoKind { Modify, Insert, Delete };
    struct UndoEntry {
        UndoKind kind = UndoKind::Modify;
        int index = -1;
        std::shared_ptr<apo::Flame> beforeFlame, afterFlame;
    };
    // Pushes `entry` onto undoStack_ (trimmed to kMaxUndoEntries) and
    // clears redoStack_ - every edit invalidates whatever could previously
    // have been redone, standard undo/redo semantics.
    void pushUndo(UndoEntry entry);
    // The shared apply logic behind both onUndo() (undoing=true: reverses
    // `entry`) and onRedo() (undoing=false: re-applies it) - see UndoKind's
    // own doc comment above for what each kind does in each direction.
    void applyUndoEntry(const UndoEntry& entry, bool undoing);

    QListWidget* flameList_ = nullptr;
    PreviewLabel* previewLabel_ = nullptr;
    QAction* copyFlameAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* editFlameAction_ = nullptr;
    QAction* duplicateFlameAction_ = nullptr;
    QAction* renameFlameAction_ = nullptr;
    QAction* deleteFlameAction_ = nullptr;
    QAction* resetLocationAction_ = nullptr;
    QAction* smoothPaletteAction_ = nullptr;
    QAction* viewThumbnailsAction_ = nullptr;
    QAction* viewListAction_ = nullptr;
    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;

    // requestRender()'s coalescing gate - see its own doc comment.
    bool renderInFlight_ = false;
    bool renderDirty_ = false;
    bool pendingCameraPreview_ = false;

    // onProgressTick()'s state - only ever populated for a full-quality
    // (non-cameraPreview) render; null/stopped otherwise. Same trio
    // RenderDialog already carries for its own (separate) progress UI.
    std::unique_ptr<apo::RenderProgress> progress_;
    QTimer* progressTimer_ = nullptr;
    QElapsedTimer elapsedTimer_;

    // Incremented on every openFlameFile() - lets a ThumbnailTask's
    // completion callback (which can arrive well after the fact, since
    // QThreadPool tasks run concurrently and asynchronously) detect that
    // the library it was rendering a thumbnail for has since been
    // replaced by a different openFlameFile() call, and discard the
    // now-stale result instead of writing it into whatever list item
    // happens to sit at the same numeric index today.
    int loadGeneration_ = 0;

    std::vector<std::shared_ptr<apo::Flame>> flames_;
    QString currentFilePath_;
    int selectedIndex_ = -1;
    QImage currentImage_;

    // A4's undo/redo stacks - bounded so a long session's history doesn't
    // grow unboundedly (each Modify/Delete entry carries a whole cloned
    // Flame). Trimmed from the front (oldest first) on overflow; see
    // pushUndo().
    static constexpr std::size_t kMaxUndoEntries = 32;
    std::vector<UndoEntry> undoStack_;
    std::vector<UndoEntry> redoStack_;

    // Brackets one camera gesture (a whole drag, or a single wheel notch)
    // into exactly one Modify undo entry: onCameraGestureStarted() (fired
    // by PreviewLabel::gestureStarted(), BEFORE any Flame::zoom/center
    // mutation - see its own doc comment for why cameraChanged() itself
    // fires too late for this) captures the "before" snapshot and sets
    // this true; onCameraChangeFinished() captures "after" and pushes the
    // entry, then resets this back to false for the next gesture. A
    // release with no preceding press-drag or wheel notch at all (e.g. a
    // gesture starting on a different, already-selected-away flame) leaves
    // this false, so no no-op/misattributed undo entry is pushed.
    bool hasPendingCameraUndo_ = false;
    int pendingCameraUndoIndex_ = -1;
    std::shared_ptr<apo::Flame> pendingCameraUndoBefore_;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
};

} // namespace apo::ui
