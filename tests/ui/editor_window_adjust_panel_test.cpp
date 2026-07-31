// Real-widget interaction test for EditorWindow's embedded Adjust panel -
// clicking the toolbar's "Adjust" action (or the right-hand panel's own
// "Adjust" tab) switches the panel switcher described in EditorWindow.h's
// class comment, instead of opening AdjustDialog as a separate top-level
// window; an AdjustPanel edit made there feeds EditorWindow's own undo
// stack exactly like a TransformPanel edit already does (see
// onXformPropertyEditingStarted/Finished).

#include <memory>

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeySequence>
#include <QStackedWidget>
#include <QTabBar>
#include <QTest>

#include "../TestHelpers.h"
#include "AdjustPanel.h"
#include "EditorWindow.h"
#include "SliderSpin.h"
#include "TransformPanel.h"
#include "core/Flame.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->name = "adjust panel embedding test flame";
    flame->width = 64;
    flame->height = 64;
    flame->zoom = 0;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    return flame;
}

void typeIntoSliderSpin(apo::ui::SliderSpin* ctrl, double value) {
    auto* spin = ctrl->findChild<QDoubleSpinBox*>();
    spin->setFocus();
    spin->setValue(value);
    QTest::keyClick(spin, Qt::Key_Return);
}

void testAdjustActionSwitchesToAdjustPanel() {
    auto* editor = new apo::ui::EditorWindow(makeTestFlame());
    editor->show();

    auto* adjustAction = editor->findChild<QAction*>("adjustAction");
    auto* tabBar = editor->findChild<QTabBar*>("rightPanelTabBar");
    auto* stack = editor->findChild<QStackedWidget*>();
    auto* transformPanel = editor->findChild<apo::ui::TransformPanel*>();
    auto* adjustPanel = editor->findChild<apo::ui::AdjustPanel*>();
    if (!check(adjustAction && tabBar && stack && transformPanel && adjustPanel,
                "EditorWindow exposes adjustAction, rightPanelTabBar, and both panels")) {
        delete editor;
        return;
    }

    check(stack->currentWidget() == transformPanel, "TransformPanel is the default page on open");

    adjustAction->trigger();
    check(tabBar->currentIndex() == 1, "triggering the toolbar's Adjust action selects the Adjust tab");
    check(stack->currentWidget() == adjustPanel, "...and switches the stack to show AdjustPanel");

    tabBar->setCurrentIndex(0);
    check(stack->currentWidget() == transformPanel, "selecting the Transform tab switches back");

    delete editor;
}

void testAdjustPanelEditFeedsEditorWindowUndoStack() {
    auto* editor = new apo::ui::EditorWindow(makeTestFlame());
    editor->show();

    auto* adjustAction = editor->findChild<QAction*>("adjustAction");
    auto* zoomCtrl = editor->findChild<apo::ui::SliderSpin*>("zoomCtrl");
    if (!check(adjustAction && zoomCtrl, "adjustAction and zoomCtrl found")) {
        delete editor;
        return;
    }
    adjustAction->trigger(); // switch to the Adjust tab, same as a real user click

    typeIntoSliderSpin(zoomCtrl, 1.5);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(editor);
    QT_WARNING_POP
    QTest::keySequence(editor, QKeySequence::Undo);
    check(approxEqual(zoomCtrl->value(), 0.0),
          "Ctrl+Z undoes an AdjustPanel edit through EditorWindow's own undo stack, and the still-visible "
          "AdjustPanel reflects the reverted value");

    QTest::keySequence(editor, QKeySequence::Redo);
    check(approxEqual(zoomCtrl->value(), 1.5), "Ctrl+Shift+Z redoes it");

    delete editor;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setApplicationName("Apophysis 7X");
    QApplication::setOrganizationName("Apophysis 7X");

    testAdjustActionSwitchesToAdjustPanel();
    testAdjustPanelEditFeedsEditorWindowUndoStack();

    return apo_test::reportAndExit();
}
