// Real interactive tests for Phase 6's Editor: simulates actual mouse
// drags (via QTest) on the live TriangleCanvas widget and checks the
// resulting xform coefficients, then does the same for EditorWindow's
// undo/redo (via the real Ctrl+Z/Ctrl+Shift+Z shortcuts). This is the gap
// flagged after the first round of Editor polish: screenshots proved the
// widget *renders* correctly, but not that dragging it actually works -
// this is what closes that.
//
// Tests check *invariants* of each drag mode rather than exact pixel-to-
// coefficient numeric values, deliberately: mouse positions are integer
// QPoints, so the achieved rotation angle/scale factor is subject to
// pixel-quantization rounding that would make exact-value assertions
// flaky. The invariants below hold regardless of that rounding (e.g. any
// rotation-around-the-centroid leaves the centroid exactly fixed, no
// matter what angle was actually achieved), so they're a robust way to
// confirm each mode does the right *kind* of thing.

#include <cmath>
#include <functional>
#include <memory>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>
#include <QTest>

#include "../TestHelpers.h"
#include "EditorWindow.h"
#include "ForceSymmetryDialog.h"
#include "TransformPanel.h"
#include "TriangleCanvas.h"
#include "XaosDialog.h"
#include "core/Flame.h"
#include "core/edit/TriangleGeometry.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 60;
    flame->height = 60;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    xf.c[0] = {0.6, 0.1};
    xf.c[1] = {-0.2, 0.5};
    xf.c[2] = {0.3, -0.2};
    xf.setVariation(0, 1.0); // linear
    return flame;
}

// Three distinguishable xforms (different color/coefs each) - for the
// Add/Duplicate/Delete tests below, which need >1 xform to exercise
// compaction and need each xform to be identifiable by its own fields.
std::shared_ptr<apo::Flame> makeMultiXformFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 60;
    flame->height = 60;
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[2] = {0.1 * i, -0.1 * i};
        xf.setVariation(0, 1.0);
    }
    return flame;
}

// Ctrl+Z/Ctrl+Shift+Z-style shortcut dispatch needs the window actually
// shown and active on the offscreen QPA platform - same reasoning (and the
// same deprecated-but-synchronous setActiveWindow() trick) as
// testEditorWindowUndoRedo() below.
void showAndActivate(apo::ui::EditorWindow* editor) {
    editor->resize(600, 500);
    editor->show();
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(editor);
    QT_WARNING_POP
}

bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

double triangleArea(const apo::Triangle& t) {
    return 0.5 * std::fabs((t.x.x - t.o.x) * (t.y.y - t.o.y) - (t.y.x - t.o.x) * (t.x.y - t.o.y));
}

// A point strictly inside the triangle, offset from the centroid (so a
// rotate/scale drag starting here has a well-defined, nonzero initial
// angle/direction relative to the centroid pivot) but not on any vertex
// or edge (so it hits the Body, not a vertex).
apo::Point2 interiorPoint(const apo::Triangle& t) {
    return {0.5 * t.o.x + 0.3 * t.x.x + 0.2 * t.y.x, 0.5 * t.o.y + 0.3 * t.x.y + 0.2 * t.y.y};
}

void testMoveDragPreservesShape() {
    auto flame = makeTestFlame();
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Move);

    const apo::Triangle before = canvas.currentTriangle(0);
    const apo::Point2 beforeXO{before.x.x - before.o.x, before.x.y - before.o.y};
    const apo::Point2 beforeYO{before.y.x - before.o.x, before.y.y - before.o.y};

    const QPoint startPx = canvas.flameToWidget(interiorPoint(before)).toPoint();
    const QPoint targetPx = startPx + QPoint(37, -22);

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(&canvas, targetPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, targetPx);

    const apo::Triangle after = canvas.currentTriangle(0);
    const apo::Point2 afterXO{after.x.x - after.o.x, after.x.y - after.o.y};
    const apo::Point2 afterYO{after.y.x - after.o.x, after.y.y - after.o.y};

    check(approxEqual(beforeXO.x, afterXO.x, 1e-6) && approxEqual(beforeXO.y, afterXO.y, 1e-6),
          "a Move body-drag preserves the O->X vector (pure translation - no shear/rotate/scale)");
    check(approxEqual(beforeYO.x, afterYO.x, 1e-6) && approxEqual(beforeYO.y, afterYO.y, 1e-6),
          "a Move body-drag preserves the O->Y vector");
    check(!(approxEqual(before.o.x, after.o.x, 1e-6) && approxEqual(before.o.y, after.o.y, 1e-6)),
          "a Move body-drag actually displaced the triangle (O changed)");
}

void testRotateDragPreservesCentroidAndArea() {
    auto flame = makeTestFlame();
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Rotate);

    const apo::Triangle before = canvas.currentTriangle(0);
    const apo::Point2 centroidBefore = apo::centroid(before);
    const double areaBefore = triangleArea(before);

    const QPoint startPx = canvas.flameToWidget(interiorPoint(before)).toPoint();
    // Drag to a point on the *opposite* side of the centroid - guarantees
    // a large, unambiguous rotation rather than a near-zero one that
    // pixel quantization could wash out.
    const QPoint centroidPx = canvas.flameToWidget(centroidBefore).toPoint();
    const QPoint targetPx = centroidPx - (startPx - centroidPx);

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(&canvas, targetPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, targetPx);

    const apo::Triangle after = canvas.currentTriangle(0);
    const apo::Point2 centroidAfter = apo::centroid(after);
    const double areaAfter = triangleArea(after);

    check(approxEqual(centroidBefore.x, centroidAfter.x, 1e-6) && approxEqual(centroidBefore.y, centroidAfter.y, 1e-6),
          "a Rotate body-drag leaves the centroid (its own pivot) exactly fixed");
    check(approxEqual(areaBefore, areaAfter, 1e-6), "a Rotate body-drag preserves the triangle's area");
    check(!(approxEqual(before.o.x, after.o.x, 1e-3) && approxEqual(before.o.y, after.o.y, 1e-3)),
          "a Rotate body-drag actually changed the triangle's orientation (O moved)");
}

void testScaleDragPreservesCentroidChangesArea() {
    auto flame = makeTestFlame();
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Scale);

    const apo::Triangle before = canvas.currentTriangle(0);
    const apo::Point2 centroidBefore = apo::centroid(before);
    const double areaBefore = triangleArea(before);

    const QPoint startPx = canvas.flameToWidget(interiorPoint(before)).toPoint();
    const QPoint centroidPx = canvas.flameToWidget(centroidBefore).toPoint();
    // Drag further out along the same ray from the pivot - a clean
    // "scale up" gesture (see scaleFactorBetween's doc comment on why
    // same-ray drags give the intuitive distance ratio).
    const QPoint targetPx = centroidPx + (startPx - centroidPx) * 2;

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(&canvas, targetPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, targetPx);

    const apo::Triangle after = canvas.currentTriangle(0);
    const apo::Point2 centroidAfter = apo::centroid(after);
    const double areaAfter = triangleArea(after);

    check(approxEqual(centroidBefore.x, centroidAfter.x, 1e-6) && approxEqual(centroidBefore.y, centroidAfter.y, 1e-6),
          "a Scale body-drag leaves the centroid (its own pivot) exactly fixed");
    check(areaAfter > areaBefore * 1.5, "dragging outward roughly doubles the area (scaling up, not down or no-op)");
}

void testCtrlDragScalesRegardlessOfCurrentMode() {
    // QTest::mouseMove(QWidget*, ...) never carries keyboard modifiers (see
    // qtestmouse.h - only the QWindow overload's caller can set stateKey),
    // so the Ctrl-held move step below goes through the lower-level
    // QTest::mouseEvent() directly instead of the mouseMove() wrapper used
    // everywhere else in this file.
    auto flame = makeTestFlame();
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Move); // deliberately not Scale mode

    const apo::Triangle before = canvas.currentTriangle(0);
    const apo::Point2 centroidBefore = apo::centroid(before);
    const double areaBefore = triangleArea(before);

    const QPoint startPx = canvas.flameToWidget(interiorPoint(before)).toPoint();
    const QPoint centroidPx = canvas.flameToWidget(centroidBefore).toPoint();
    const QPoint targetPx = centroidPx + (startPx - centroidPx) * 2;

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::ControlModifier, startPx);
    QTest::mouseEvent(QTest::MouseMove, &canvas, Qt::NoButton, Qt::ControlModifier, targetPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::ControlModifier, targetPx);

    const apo::Triangle after = canvas.currentTriangle(0);
    const apo::Point2 centroidAfter = apo::centroid(after);
    const double areaAfter = triangleArea(after);

    check(canvas.editMode() == apo::ui::TriangleCanvas::EditMode::Move,
          "the global toolbar mode itself is untouched by a Ctrl-held drag - Ctrl overrides for that one drag only");
    check(approxEqual(centroidBefore.x, centroidAfter.x, 1e-6) && approxEqual(centroidBefore.y, centroidAfter.y, 1e-6),
          "a Ctrl-held body-drag scales around the centroid, exactly like Scale mode, even while Move is active");
    check(areaAfter > areaBefore * 1.5,
          "a Ctrl-held drag outward roughly doubles the area - it scaled, it didn't move or rotate");
}

void testSingleVertexMoveOnlyMovesThatVertex() {
    auto flame = makeTestFlame();
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Move);

    const apo::Triangle before = canvas.currentTriangle(0);
    const QPoint xPx = canvas.flameToWidget(before.x).toPoint();
    const QPoint targetPx = xPx + QPoint(15, 20);

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, xPx);
    QTest::mouseMove(&canvas, targetPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, targetPx);

    const apo::Triangle after = canvas.currentTriangle(0);
    check(approxEqual(before.o.x, after.o.x, 1e-9) && approxEqual(before.o.y, after.o.y, 1e-9),
          "grabbing the X vertex alone leaves O exactly untouched");
    check(approxEqual(before.y.x, after.y.x, 1e-9) && approxEqual(before.y.y, after.y.y, 1e-9),
          "grabbing the X vertex alone leaves Y exactly untouched");
    check(!(approxEqual(before.x.x, after.x.x, 1e-6) && approxEqual(before.x.y, after.x.y, 1e-6)),
          "grabbing the X vertex alone actually moves X");
}

void testPivotPickChangesRotateResult() {
    // Runs the exact same rotate gesture twice on independent flame
    // copies - once with the default (centroid) pivot, once after
    // arming pivot-pick and clicking a point well away from the
    // centroid - and confirms the two results differ, proving the picked
    // pivot actually feeds into the drag math rather than being cosmetic.
    auto runRotateDrag = [](std::shared_ptr<apo::Flame> flame, bool useCustomPivot) {
        apo::ui::TriangleCanvas canvas;
        canvas.resize(500, 500);
        canvas.setFlame(flame);
        canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Rotate);

        const apo::Triangle t = canvas.currentTriangle(0);
        if (useCustomPivot) {
            canvas.setPivotPickArmed(true);
            const apo::Point2 farPivot{t.o.x + 5.0, t.o.y + 5.0}; // well outside the triangle
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, canvas.flameToWidget(farPivot).toPoint());
        }

        const QPoint startPx = canvas.flameToWidget(interiorPoint(t)).toPoint();
        const QPoint targetPx = startPx + QPoint(-40, 30);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, startPx);
        QTest::mouseMove(&canvas, targetPx);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, targetPx);

        return canvas.currentTriangle(0);
    };

    const apo::Triangle withCentroidPivot = runRotateDrag(makeTestFlame(), false);
    const apo::Triangle withCustomPivot = runRotateDrag(makeTestFlame(), true);

    const bool differ = !approxEqual(withCentroidPivot.o.x, withCustomPivot.o.x, 1e-3) ||
                         !approxEqual(withCentroidPivot.o.y, withCustomPivot.o.y, 1e-3);
    check(differ, "the same rotate gesture produces a different result once a custom pivot is picked");
}

void testEditingSignalsFireWithCorrectIndexAndCount() {
    auto flame = makeTestFlame();
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Move);

    QSignalSpy startedSpy(&canvas, &apo::ui::TriangleCanvas::editingStarted);
    QSignalSpy editedSpy(&canvas, &apo::ui::TriangleCanvas::xformEdited);
    QSignalSpy finishedSpy(&canvas, &apo::ui::TriangleCanvas::editingFinished);

    const apo::Triangle t = canvas.currentTriangle(0);
    const QPoint startPx = canvas.flameToWidget(interiorPoint(t)).toPoint();
    const QPoint midPx = startPx + QPoint(10, 5);
    const QPoint endPx = startPx + QPoint(20, 10);

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(&canvas, midPx);
    QTest::mouseMove(&canvas, endPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, endPx);

    check(startedSpy.count() == 1, "editingStarted fires exactly once per drag");
    check(editedSpy.count() == 2, "xformEdited fires once per mouse-move during the drag (two moves here)");
    check(finishedSpy.count() == 1, "editingFinished fires exactly once, on release");

    if (startedSpy.count() == 1) {
        check(startedSpy.at(0).at(0).toInt() == 0, "editingStarted reports the correct xform index");
    }
    if (finishedSpy.count() == 1) {
        check(finishedSpy.at(0).at(0).toInt() == 0, "editingFinished reports the correct xform index");
    }
}

void testEditorWindowUndoRedo() {
    auto flame = makeTestFlame();
    const auto originalC = flame->xform[0]->c;

    auto* editor = new apo::ui::EditorWindow(flame);
    editor->resize(600, 500);
    // Ctrl+Z/Ctrl+Shift+Z below are QAction shortcuts (Qt::WindowShortcut
    // context, the default) - unlike raw mouse events, shortcut dispatch
    // depends on window activation state, so the window needs to actually
    // be shown (even on the offscreen QPA platform - see main() below -
    // "shown" still means "participates in activation tracking", just
    // without drawing real pixels) for QTest::keySequence to reach it.
    editor->show();
    // QApplication::setActiveWindow is deprecated in favor of
    // QWidget::activateWindow(), but on the offscreen QPA platform (no real
    // window manager) activateWindow() doesn't synchronously update
    // QApplication's active-window state, so QTest::keySequence's shortcut
    // dispatch below doesn't reliably reach the window. setActiveWindow()
    // does the synchronous update this headless test needs.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(editor);
    QT_WARNING_POP

    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    if (!check(canvas != nullptr, "EditorWindow contains a TriangleCanvas child")) {
        delete editor;
        return;
    }

    const apo::Triangle t = canvas->currentTriangle(0);
    const QPoint startPx = canvas->flameToWidget(interiorPoint(t)).toPoint();
    const QPoint targetPx = startPx + QPoint(30, -25);

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(canvas, targetPx);
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, targetPx);

    const auto editedC = flame->xform[0]->c;
    check(editedC != originalC, "dragging through EditorWindow's canvas actually edits the flame");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(flame->xform[0]->c == originalC, "Ctrl+Z reverts the coefficients to their pre-drag values");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(flame->xform[0]->c == editedC, "Ctrl+Shift+Z re-applies the drag's post-edit coefficients");

    delete editor;
}

void testAddXformActionAppendsAndSelectsNewXform() {
    auto flame = makeMultiXformFlame();
    auto* editor = new apo::ui::EditorWindow(flame);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* addAction = editor->findChild<QAction*>("addXformAction");
    if (!check(canvas && addAction, "TriangleCanvas and addXformAction found")) {
        delete editor;
        return;
    }

    addAction->trigger();

    check(flame->numXForms() == 4, "triggering Add appends a new xform (3 -> 4)");
    check(approxEqual(flame->xform[3]->density, 0.5), "the new xform defaults to density 0.5, matching addXForm()");
    check(canvas->selectedXform() == 3, "Add selects the newly appended xform");

    delete editor;
}

void testDuplicateViaInsertKeyPreservesXaosLinks() {
    auto flame = makeMultiXformFlame();
    flame->xform[0]->modWeights[1] = 0.4; // 0 -> 1, will be duplicated to 0 -> 3
    flame->xform[1]->color = 0.75;

    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    if (!check(canvas != nullptr, "TriangleCanvas found")) {
        delete editor;
        return;
    }
    canvas->setSelectedXform(1);

    // Matches Editor.pas's TriangleViewKeyDown: VK_INSERT triggers
    // Duplicate.
    QTest::keyClick(editor, Qt::Key_Insert);

    check(flame->numXForms() == 4, "Insert duplicates the selected xform (3 -> 4)");
    check(approxEqual(flame->xform[3]->color, 0.75), "the duplicate is a full copy of xform 1's own fields");
    check(approxEqual(flame->xform[0]->modWeights[3], flame->xform[0]->modWeights[1]),
          "xform 0's weight toward the duplicate matches its weight toward the original (xform 1)");
    check(canvas->selectedXform() == 3, "Duplicate selects the newly appended copy");

    delete editor;
}

void testDeleteViaDeleteKeyCompactsRemainingXforms() {
    auto flame = makeMultiXformFlame(); // colors 0, 0.5, 1
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* xformList = editor->findChild<QListWidget*>();
    if (!check(canvas && xformList, "TriangleCanvas and xform list found")) {
        delete editor;
        return;
    }
    canvas->setSelectedXform(0); // delete the first xform (color 0)

    QTest::keyClick(editor, Qt::Key_Delete);

    check(flame->numXForms() == 2, "Delete removes the selected xform (3 -> 2)");
    check(approxEqual(flame->xform[0]->color, 0.5) && approxEqual(flame->xform[1]->color, 1.0),
          "the surviving xforms are re-packed in order after the deleted one");
    check(xformList->count() == 2, "the xform list shrinks to match the new xform count");
    check(canvas->selectedXform() >= 0 && canvas->selectedXform() < 2, "selection lands on a valid remaining xform");

    delete editor;
}

void testDeleteRefusesToRemoveLastXform() {
    auto flame = makeTestFlame(); // single xform
    flame->xform[0]->density = 1.0;
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);

    QTest::keyClick(editor, Qt::Key_Delete);

    check(flame->numXForms() == 1, "Delete on a flame's last remaining xform is a no-op");

    delete editor;
}

void testStructuralUndoRedoRoundTrips() {
    auto flame = makeMultiXformFlame();
    auto* editor = new apo::ui::EditorWindow(flame);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* addAction = editor->findChild<QAction*>("addXformAction");
    if (!check(canvas && addAction, "TriangleCanvas and addXformAction found")) {
        delete editor;
        return;
    }
    showAndActivate(editor);

    addAction->trigger();
    check(flame->numXForms() == 4, "Add applied before the undo/redo round trip");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(flame->numXForms() == 3, "Ctrl+Z undoes Add, restoring the original xform count");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(flame->numXForms() == 4, "Ctrl+Shift+Z redoes Add, restoring the added xform");

    delete editor;
}

void testDragThenStructuralUndoUnwindInOrder() {
    // A drag (per-xform coefficient undo entry) immediately followed by a
    // structural Add (whole-flame undo entry) - confirms the two entry
    // kinds correctly share one undo stack in the order they actually
    // happened, not just each in isolation.
    auto flame = makeMultiXformFlame();
    const auto originalC = flame->xform[0]->c;

    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* addAction = editor->findChild<QAction*>("addXformAction");
    if (!check(canvas && addAction, "TriangleCanvas and addXformAction found")) {
        delete editor;
        return;
    }
    canvas->setSelectedXform(0);

    const apo::Triangle t = canvas->currentTriangle(0);
    const QPoint startPx = canvas->flameToWidget(interiorPoint(t)).toPoint();
    const QPoint targetPx = startPx + QPoint(20, -15);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(canvas, targetPx);
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, targetPx);
    const auto draggedC = flame->xform[0]->c;
    check(draggedC != originalC, "the drag actually changed xform 0's coefficients");

    addAction->trigger();
    check(flame->numXForms() == 4, "Add applied on top of the drag");

    QTest::keySequence(editor, QKeySequence::Undo); // undoes Add
    check(flame->numXForms() == 3 && flame->xform[0]->c == draggedC,
          "the first Undo reverts only the most recent action (Add), leaving the earlier drag intact");

    QTest::keySequence(editor, QKeySequence::Undo); // undoes the drag
    check(flame->xform[0]->c == originalC, "the second Undo reverts the drag underneath it");

    delete editor;
}

void testXaosDialogOpensReusesAndRefreshesOnStructuralEdit() {
    auto flame = makeMultiXformFlame(); // 3 xforms
    auto* editor = new apo::ui::EditorWindow(flame);
    auto* xaosAction = editor->findChild<QAction*>("xaosAction");
    auto* addAction = editor->findChild<QAction*>("addXformAction");
    if (!check(xaosAction && addAction, "Xaos and Add actions found")) {
        delete editor;
        return;
    }

    xaosAction->trigger();
    auto* xaosDialog = editor->findChild<apo::ui::XaosDialog*>();
    if (!check(xaosDialog != nullptr, "triggering the Xaos action opens a XaosDialog child")) {
        delete editor;
        return;
    }
    auto* table = xaosDialog->findChild<QTableWidget*>("xaosTable");
    if (!check(table != nullptr, "the opened XaosDialog exposes its table")) {
        delete editor;
        return;
    }
    check(table->rowCount() == 3, "the table starts sized to the flame's 3 xforms");

    xaosAction->trigger(); // triggering again while already open should reuse, not duplicate
    check(editor->findChildren<apo::ui::XaosDialog*>().size() == 1,
          "triggering Xaos again while one is already open reuses it instead of opening a second instance");

    addAction->trigger(); // structural edit made through EditorWindow, not the Xaos dialog itself
    check(table->rowCount() == 4,
          "an Add xform performed elsewhere in EditorWindow live-refreshes the still-open Xaos dialog's table "
          "(EditorWindow::refreshXaosDialogIfOpen)");

    delete editor;
}

void testTriangleCanvasSetSelectedXformRejectsFinalWhenDisabled() {
    auto flame = makeMultiXformFlame();
    flame->finalXformEnabled = false;
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setSelectedXform(1);

    canvas.setSelectedXform(apo::kFinalXformIndex);
    check(canvas.selectedXform() == 1,
          "setSelectedXform(kFinalXformIndex) is a no-op when Flame::finalXformEnabled is false");
}

void testTriangleCanvasSelectsAndDragsTheFinalTransform() {
    auto flame = makeMultiXformFlame(); // 3 regular xforms
    flame->finalXformEnabled = true;
    flame->finalXform->clear();
    flame->finalXform->c[2] = {5.0, 5.0}; // placed well away from every regular xform's triangle
    const auto originalXform0C = flame->xform[0]->c;

    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.fitView(); // include the final triangle in the fitted bounds
    canvas.setEditMode(apo::ui::TriangleCanvas::EditMode::Move);
    canvas.setSelectedXform(apo::kFinalXformIndex);
    check(canvas.selectedXform() == apo::kFinalXformIndex, "the final transform can be selected");

    const apo::Triangle before = canvas.currentTriangle(apo::kFinalXformIndex);
    const QPoint startPx = canvas.flameToWidget(interiorPoint(before)).toPoint();
    const QPoint targetPx = startPx + QPoint(30, -18);

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(&canvas, targetPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, targetPx);

    const apo::Triangle after = canvas.currentTriangle(apo::kFinalXformIndex);
    check(!(approxEqual(before.o.x, after.o.x, 1e-6) && approxEqual(before.o.y, after.o.y, 1e-6)),
          "dragging the final transform's triangle actually moves it (XForm::finalXform->c changed)");
    check(flame->xform[0]->c == originalXform0C, "dragging the final transform leaves every regular xform untouched");
}

void testTriangleCanvasClickToSelectTheFinalTransform() {
    auto flame = makeMultiXformFlame();
    flame->finalXformEnabled = true;
    flame->finalXform->clear();
    flame->finalXform->c[2] = {5.0, 5.0};

    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.fitView();
    canvas.setSelectedXform(0); // start on a regular xform, not the final one

    QSignalSpy spy(&canvas, &apo::ui::TriangleCanvas::selectedXformChanged);
    const apo::Triangle finalTriangle = canvas.currentTriangle(apo::kFinalXformIndex);
    const QPoint originPx = canvas.flameToWidget(finalTriangle.o).toPoint();

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);

    check(canvas.selectedXform() == apo::kFinalXformIndex, "clicking the final triangle's O vertex selects it");
    check(spy.count() == 1 && spy.at(0).at(0).toInt() == apo::kFinalXformIndex,
          "selectedXformChanged fires once with kFinalXformIndex");
}

void testOverlappingTrianglesCycleSelectionOnRepeatedClicks() {
    auto flame = makeMultiXformFlame(); // 3 xforms, xform 2 offset well away from 0/1
    flame->xform[1]->c = flame->xform[0]->c; // xforms 0 and 1 now share the exact same triangle
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setSelectedXform(2); // start elsewhere, so the first click below is a fresh pick

    const QPoint originPx = canvas.flameToWidget(canvas.currentTriangle(0).o).toPoint();

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    check(canvas.selectedXform() == 0, "the first click on a shared vertex selects the first (lowest-index) overlapping xform");

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    check(canvas.selectedXform() == 1, "clicking again at the same spot cycles to the next overlapping xform");

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    check(canvas.selectedXform() == 0, "a third click at the same spot wraps back around to the first overlapping xform");
}

void testClickCyclingResetsAfterClickingElsewhereOrDragging() {
    auto flame = makeMultiXformFlame();
    flame->xform[1]->c = flame->xform[0]->c; // xforms 0 and 1 share the exact same triangle
    apo::ui::TriangleCanvas canvas;
    canvas.resize(500, 500);
    canvas.setFlame(flame);
    canvas.setSelectedXform(2);

    const QPoint originPx = canvas.flameToWidget(canvas.currentTriangle(0).o).toPoint();
    const QPoint xVertexPx = canvas.flameToWidget(canvas.currentTriangle(2).x).toPoint();

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    check(canvas.selectedXform() == 0, "first click on the overlap selects xform 0");

    // An unrelated click elsewhere (xform 2's own, non-overlapping vertex)
    // shouldn't be part of the overlap sequence.
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, xVertexPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, xVertexPx);
    check(canvas.selectedXform() == 2, "a click elsewhere selects that triangle instead of continuing the old cycle");

    // Clicking back on the overlap starts the cycle over, not wherever it
    // left off two clicks ago.
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    check(canvas.selectedXform() == 0, "returning to the overlap point restarts the cycle at the first candidate");

    // Now do a genuine drag starting on the overlap point that moves well
    // away and then back to exactly where it started - real pointer
    // motion happened, so this must still count as a drag, not a click,
    // even though the net displacement between press and release is zero.
    const QPoint awayPx = originPx + QPoint(25, 15);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseMove(&canvas, awayPx);
    QTest::mouseMove(&canvas, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, originPx);
    check(canvas.selectedXform() == 0,
          "the click right after a drag - even one that returns to its start point - restarts the cycle at the "
          "first candidate, proving net-zero displacement doesn't get mistaken for a stationary click");
}

void testFinalTransformToggleThroughEditorWindowWithUndo() {
    auto flame = makeMultiXformFlame(); // 3 xforms
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* xformList = editor->findChild<QListWidget*>();
    auto* finalAction = editor->findChild<QAction*>("finalXformAction");
    if (!check(canvas && xformList && finalAction, "TriangleCanvas, xform list, and finalXformAction found")) {
        delete editor;
        return;
    }

    check(!flame->finalXformEnabled, "the final transform starts disabled");
    check(xformList->count() == 3, "the xform list starts with exactly one row per regular xform, no Final row");

    finalAction->trigger();
    check(flame->finalXformEnabled && flame->useFinalXform,
          "triggering Final Transform enables both Flame::finalXformEnabled and useFinalXform");
    check(xformList->count() == 4, "a trailing \"Final\" row is appended to the xform list");
    check(canvas->selectedXform() == apo::kFinalXformIndex, "enabling it also selects it, like Add/Duplicate do");
    check(xformList->currentRow() == 3, "the list selection follows onto the new Final row");

    finalAction->trigger(); // disable again
    check(!flame->finalXformEnabled, "triggering Final Transform again disables it");
    check(xformList->count() == 3, "the Final row is removed once disabled");
    check(canvas->selectedXform() == 0, "deselecting the final transform falls back to xform 0");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(flame->finalXformEnabled, "Ctrl+Z undoes disabling the final transform, restoring it");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(!flame->finalXformEnabled, "Ctrl+Shift+Z redoes disabling the final transform");

    delete editor;
}

void testCopyPasteTransformThroughEditorWindowWithUndo() {
    auto flame = makeMultiXformFlame(); // 3 distinguishable xforms
    flame->xform[0]->transformName = "Source";
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* copyAction = editor->findChild<QAction*>("copyXformAction");
    auto* pasteAction = editor->findChild<QAction*>("pasteXformAction");
    if (!check(canvas && copyAction && pasteAction, "TriangleCanvas, copyXformAction, and pasteXformAction found")) {
        delete editor;
        return;
    }

    canvas->setSelectedXform(0);
    copyAction->trigger();

    const auto originalXform2C = flame->xform[2]->c;
    canvas->setSelectedXform(2);
    pasteAction->trigger();

    check(flame->xform[2]->transformName == "Source", "pasting copies the source xform's name onto the selected one");
    check(flame->xform[2]->c == flame->xform[0]->c, "pasting copies the source xform's affine coefficients too");
    check(flame->xform[2]->c != originalXform2C, "the target xform's own original coefficients were overwritten");
    check(flame->xform[0]->transformName == "Source", "copying doesn't modify the source xform");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(flame->xform[2]->c == originalXform2C, "Ctrl+Z undoes the paste, restoring xform 2's original coefficients");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(flame->xform[2]->c == flame->xform[0]->c, "Ctrl+Shift+Z redoes the paste");

    delete editor;
}

void testPasteOntoTheFinalTransform() {
    auto flame = makeMultiXformFlame();
    flame->xform[1]->transformName = "CopyMe";
    flame->finalXformEnabled = true;
    flame->finalXform->clear();
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* copyAction = editor->findChild<QAction*>("copyXformAction");
    auto* pasteAction = editor->findChild<QAction*>("pasteXformAction");
    if (!check(canvas && copyAction && pasteAction, "TriangleCanvas, copyXformAction, and pasteXformAction found")) {
        delete editor;
        return;
    }

    canvas->setSelectedXform(1);
    copyAction->trigger();
    canvas->setSelectedXform(apo::kFinalXformIndex);
    pasteAction->trigger();

    check(flame->finalXform->transformName == "CopyMe", "pasting works onto the final transform too");
    check(flame->finalXform->c == flame->xform[1]->c, "the final transform's coefficients were overwritten by the paste");

    delete editor;
}

void testForceSymmetryAppliesThroughEditorWindowWithUndo() {
    auto flame = makeMultiXformFlame(); // 3 xforms
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
    auto* forceSymmetryAction = editor->findChild<QAction*>("forceSymmetryAction");
    if (!check(canvas && forceSymmetryAction, "TriangleCanvas and forceSymmetryAction found")) {
        delete editor;
        return;
    }

    forceSymmetryAction->trigger();
    auto* dialog = editor->findChild<apo::ui::ForceSymmetryDialog*>();
    if (!check(dialog != nullptr, "triggering Force Symmetry opens a ForceSymmetryDialog child")) {
        delete editor;
        return;
    }
    auto* kindCombo = dialog->findChild<QComboBox*>("kindCombo");
    auto* orderSpin = dialog->findChild<QSpinBox*>("orderSpin");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(kindCombo && orderSpin && applyButton, "dialog controls found")) {
        delete editor;
        return;
    }

    kindCombo->setCurrentIndex(0); // Rotational
    orderSpin->setValue(4);
    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(flame->numXForms() == 6, "applying rotational order-4 symmetry appends 3 xforms (3 -> 6)");
    check(canvas->selectedXform() == 3, "the selection lands on the first newly-appended symmetry xform");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(flame->numXForms() == 3, "Ctrl+Z undoes Force Symmetry, restoring the original xform count");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(flame->numXForms() == 6, "Ctrl+Shift+Z redoes Force Symmetry");

    delete editor;
}

// A6's Flame-menu weight/color utility commands (core/edit/FlameOps.h) are
// already covered independently in flame_ops_test.cpp; these four verify
// the Qt wiring on top of them - each menu action reaches the real flame_
// EditorWindow owns, and participates in the same Ctrl+Z/Ctrl+Shift+Z
// undo/redo as Force Symmetry above.

void testRandomizeWeightsAppliesThroughEditorWindowWithUndo() {
    auto flame = makeMultiXformFlame(); // 3 xforms, density=1.0 each
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* action = editor->findChild<QAction*>("randomizeWeightsAction");
    if (!check(action != nullptr, "randomizeWeightsAction found")) {
        delete editor;
        return;
    }

    const double d0 = flame->xform[0]->density, d1 = flame->xform[1]->density, d2 = flame->xform[2]->density;
    action->trigger();
    const double a0 = flame->xform[0]->density, a1 = flame->xform[1]->density, a2 = flame->xform[2]->density;

    check(!approxEqual(a0, d0) || !approxEqual(a1, d1) || !approxEqual(a2, d2),
          "triggering Random Weights changes at least one xform's density");
    check(a0 >= 0.0 && a0 < 1.0 && a1 >= 0.0 && a1 < 1.0 && a2 >= 0.0 && a2 < 1.0,
          "Random Weights sets every active xform's density into [0, 1)");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(approxEqual(flame->xform[0]->density, d0) && approxEqual(flame->xform[1]->density, d1) &&
              approxEqual(flame->xform[2]->density, d2),
          "Ctrl+Z undoes Random Weights, restoring the original densities");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(approxEqual(flame->xform[0]->density, a0) && approxEqual(flame->xform[1]->density, a1) &&
              approxEqual(flame->xform[2]->density, a2),
          "Ctrl+Shift+Z redoes Random Weights, reproducing the exact same randomized densities");

    delete editor;
}

void testEqualizeWeightsAppliesThroughEditorWindowWithUndo() {
    auto flame = makeMultiXformFlame();
    flame->xform[0]->density = 0.2;
    flame->xform[1]->density = 0.7;
    flame->xform[2]->density = 0.9;
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* action = editor->findChild<QAction*>("equalizeWeightsAction");
    if (!check(action != nullptr, "equalizeWeightsAction found")) {
        delete editor;
        return;
    }

    action->trigger();
    check(approxEqual(flame->xform[0]->density, 0.5) && approxEqual(flame->xform[1]->density, 0.5) &&
              approxEqual(flame->xform[2]->density, 0.5),
          "triggering Equalize Weights sets every active xform's density to 0.5");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(approxEqual(flame->xform[0]->density, 0.2) && approxEqual(flame->xform[1]->density, 0.7) &&
              approxEqual(flame->xform[2]->density, 0.9),
          "Ctrl+Z undoes Equalize Weights, restoring the original (unequal) densities");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(approxEqual(flame->xform[0]->density, 0.5) && approxEqual(flame->xform[1]->density, 0.5) &&
              approxEqual(flame->xform[2]->density, 0.5),
          "Ctrl+Shift+Z redoes Equalize Weights");

    delete editor;
}

void testCalculateColorValuesAppliesThroughEditorWindowWithUndo() {
    auto flame = makeMultiXformFlame(); // colors 0, 0.5, 1 - already matches, so use distinctive starting colors
    flame->xform[0]->color = 0.9;
    flame->xform[1]->color = 0.9;
    flame->xform[2]->color = 0.9;
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* action = editor->findChild<QAction*>("calculateColorsAction");
    if (!check(action != nullptr, "calculateColorsAction found")) {
        delete editor;
        return;
    }

    action->trigger();
    check(approxEqual(flame->xform[0]->color, 0.0) && approxEqual(flame->xform[1]->color, 0.5) &&
              approxEqual(flame->xform[2]->color, 1.0),
          "triggering Calculate Colors Values evenly spreads the 3 xforms' colors across [0, 1]");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(approxEqual(flame->xform[0]->color, 0.9) && approxEqual(flame->xform[1]->color, 0.9) &&
              approxEqual(flame->xform[2]->color, 0.9),
          "Ctrl+Z undoes Calculate Colors Values, restoring the original colors");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(approxEqual(flame->xform[0]->color, 0.0) && approxEqual(flame->xform[1]->color, 0.5) &&
              approxEqual(flame->xform[2]->color, 1.0),
          "Ctrl+Shift+Z redoes Calculate Colors Values");

    delete editor;
}

void testRandomizeColorValuesAppliesThroughEditorWindowWithUndo() {
    auto flame = makeMultiXformFlame();
    flame->xform[0]->color = 0.5;
    flame->xform[1]->color = 0.5;
    flame->xform[2]->color = 0.5;
    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* action = editor->findChild<QAction*>("randomizeColorsAction");
    if (!check(action != nullptr, "randomizeColorsAction found")) {
        delete editor;
        return;
    }

    action->trigger();
    const double a0 = flame->xform[0]->color, a1 = flame->xform[1]->color, a2 = flame->xform[2]->color;
    check(!approxEqual(a0, 0.5) || !approxEqual(a1, 0.5) || !approxEqual(a2, 0.5),
          "triggering Randomize Color Values changes at least one xform's color");
    check(a0 >= 0.0 && a0 < 1.0 && a1 >= 0.0 && a1 < 1.0 && a2 >= 0.0 && a2 < 1.0,
          "Randomize Color Values sets every active xform's color into [0, 1)");

    QTest::keySequence(editor, QKeySequence::Undo);
    check(approxEqual(flame->xform[0]->color, 0.5) && approxEqual(flame->xform[1]->color, 0.5) &&
              approxEqual(flame->xform[2]->color, 0.5),
          "Ctrl+Z undoes Randomize Color Values, restoring the original colors");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(approxEqual(flame->xform[0]->color, a0) && approxEqual(flame->xform[1]->color, a1) &&
              approxEqual(flame->xform[2]->color, a2),
          "Ctrl+Shift+Z redoes Randomize Color Values, reproducing the exact same randomized colors");

    delete editor;
}

void testEditorPreviewRendersAtReducedResolutionAndDensity() {
    // A flame authored for real production output - large canvas, a
    // genuinely expensive sample density - the exact scenario that used to
    // make every triangle drag re-render at full quality on every mouse-
    // move (see EditorWindow.h's class comment on requestRender()).
    auto flame = makeMultiXformFlame();
    flame->width = 1920;
    flame->height = 1080;
    flame->sampleDensity = 500;

    auto* editor = new apo::ui::EditorWindow(flame);
    editor->show();
    QSignalSpy spy(editor, &apo::ui::EditorWindow::renderRequested);

    // Guarantees a fresh requestRender() call regardless of whether the
    // constructor's own initial render (already emitted and missed by this
    // spy, attached after construction) is still in flight.
    editor->resize(400, 300);

    const bool gotSignal = waitUntil([&spy] { return spy.count() >= 1; });
    if (!check(gotSignal, "resizing the Editor window triggers a fresh preview render request")) {
        delete editor;
        return;
    }

    const auto previewFlame = spy.at(0).at(0).value<std::shared_ptr<const apo::Flame>>();
    if (check(previewFlame != nullptr, "the emitted preview flame is non-null")) {
        auto* canvas = editor->findChild<apo::ui::TriangleCanvas*>();
        // Compared against the canvas's own actual on-screen size, not a
        // hardcoded 400x300: the 400x300 passed to resize() above is
        // itself only a request - it's clamped up to the window's real
        // layout-minimum size (menus/toolbars/panels all contribute to
        // that floor), which varies by platform/font metrics. Observed
        // concretely landing above 400x300 on macOS's offscreen QPA
        // platform even though this same test settles at exactly 400x300
        // on Linux/Windows. The invariant this test actually cares about
        // is preview-tracks-canvas, not preview-equals-400x300.
        check(canvas != nullptr && previewFlame->width <= canvas->width() && previewFlame->height <= canvas->height(),
              "the Editor's live preview renders at a resolution fitted to the canvas, not the flame's own huge "
              "declared width/height (1920x1080)");
        check(previewFlame->width < 1920 && previewFlame->height < 1080,
              "the Editor's live preview resolution is meaningfully reduced from the flame's own declared size");
        check(previewFlame->sampleDensity < 500,
              "the Editor's live preview uses a reduced preview sample density, not the flame's own expensive "
              "value (500)");
    }
    check(flame->width == 1920 && flame->height == 1080 && approxEqual(flame->sampleDensity, 500.0),
          "the preview clone never mutates the original flame's own width/height/sampleDensity");

    delete editor;
}

void testTransformPanelTracksListSelection() {
    auto flame = makeMultiXformFlame(); // 3 xforms, distinct colors
    auto* editor = new apo::ui::EditorWindow(flame);
    auto* panel = editor->findChild<apo::ui::TransformPanel*>();
    auto* xformList = editor->findChild<QListWidget*>();
    if (!check(panel && xformList, "EditorWindow exposes a TransformPanel and an xform list")) {
        delete editor;
        return;
    }
    check(panel->selectedXform() == 0, "the panel starts tracking xform 0, matching the Editor's initial selection");

    xformList->setCurrentRow(2);
    check(panel->selectedXform() == 2, "selecting a different xform in the list re-targets the panel");

    delete editor;
}

void testTransformPanelWeightEditIntegratesWithUndo() {
    auto flame = makeMultiXformFlame();
    const double originalWeight = flame->xform[0]->density;

    auto* editor = new apo::ui::EditorWindow(flame);
    showAndActivate(editor);
    auto* panel = editor->findChild<apo::ui::TransformPanel*>();
    auto* weightSpin = panel ? panel->findChild<QDoubleSpinBox*>("transformWeightSpin") : nullptr;
    if (!check(panel && weightSpin, "TransformPanel and its weight spin found")) {
        delete editor;
        return;
    }

    weightSpin->setValue(originalWeight + 5.0);
    check(approxEqual(flame->xform[0]->density, originalWeight + 5.0),
          "editing the weight spin box through the Editor reaches the actual flame");

    weightSpin->setFocus();
    QTest::keyClick(weightSpin, Qt::Key_Return); // commits the gesture -> pushes an undo entry

    QTest::keySequence(editor, QKeySequence::Undo);
    check(approxEqual(flame->xform[0]->density, originalWeight),
          "Ctrl+Z undoes a TransformPanel weight edit, matching how it undoes a triangle drag");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(approxEqual(flame->xform[0]->density, originalWeight + 5.0), "Ctrl+Shift+Z redoes the weight edit");

    delete editor;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testMoveDragPreservesShape();
    testRotateDragPreservesCentroidAndArea();
    testScaleDragPreservesCentroidChangesArea();
    testCtrlDragScalesRegardlessOfCurrentMode();
    testSingleVertexMoveOnlyMovesThatVertex();
    testPivotPickChangesRotateResult();
    testEditingSignalsFireWithCorrectIndexAndCount();
    testEditorWindowUndoRedo();
    testAddXformActionAppendsAndSelectsNewXform();
    testDuplicateViaInsertKeyPreservesXaosLinks();
    testDeleteViaDeleteKeyCompactsRemainingXforms();
    testDeleteRefusesToRemoveLastXform();
    testStructuralUndoRedoRoundTrips();
    testDragThenStructuralUndoUnwindInOrder();
    testXaosDialogOpensReusesAndRefreshesOnStructuralEdit();
    testTriangleCanvasSetSelectedXformRejectsFinalWhenDisabled();
    testTriangleCanvasSelectsAndDragsTheFinalTransform();
    testTriangleCanvasClickToSelectTheFinalTransform();
    testOverlappingTrianglesCycleSelectionOnRepeatedClicks();
    testClickCyclingResetsAfterClickingElsewhereOrDragging();
    testFinalTransformToggleThroughEditorWindowWithUndo();
    testCopyPasteTransformThroughEditorWindowWithUndo();
    testPasteOntoTheFinalTransform();
    testForceSymmetryAppliesThroughEditorWindowWithUndo();
    testRandomizeWeightsAppliesThroughEditorWindowWithUndo();
    testEqualizeWeightsAppliesThroughEditorWindowWithUndo();
    testCalculateColorValuesAppliesThroughEditorWindowWithUndo();
    testRandomizeColorValuesAppliesThroughEditorWindowWithUndo();
    testEditorPreviewRendersAtReducedResolutionAndDensity();
    testTransformPanelTracksListSelection();
    testTransformPanelWeightEditIntegratesWithUndo();

    return apo_test::reportAndExit();
}
