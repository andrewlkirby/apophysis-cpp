// Real-widget interaction tests for XaosDialog (src/ui/XaosDialog.h) - the
// underlying compaction math (Flame::addXForm/duplicateXForm/removeXForm)
// is already covered independently in core_smoke_test.cpp; this file
// verifies the Qt wiring on top of it: the table starts populated from the
// flame's own weight matrix with colored per-xform headers, editing a cell
// reaches the Flame and emits flameChanged, invalid/negative input is
// rejected/clamped, Clear All / Set All apply to the whole matrix, and
// refreshFromFlame() picks up a structural edit (Add/Delete) made
// elsewhere while the dialog stays open - the scenario
// EditorWindow::openXaosDialog's live-pointer wiring exists for. Mirrors
// curves_dialog_interaction_test.cpp's approach.

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>

#include "../TestHelpers.h"
#include "XaosDialog.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeThreeXformFlame() {
    auto flame = std::make_shared<apo::Flame>();
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
    }
    flame->xform[0]->modWeights[1] = 0.25;
    flame->xform[1]->modWeights[2] = 0.75;
    flame->xform[2]->modWeights[0] = 0.5;
    return flame;
}

void testTableStartsPopulatedFromFlame() {
    auto flame = makeThreeXformFlame();
    auto* dialog = new apo::ui::XaosDialog(flame);

    auto* table = dialog->findChild<QTableWidget*>("xaosTable");
    if (!check(table != nullptr, "XaosDialog exposes an xaosTable")) {
        delete dialog;
        return;
    }

    check(table->rowCount() == 3 && table->columnCount() == 3, "the table is sized to the flame's xform count (3x3)");
    check(table->item(0, 1)->text().toDouble() == 0.25, "cell [0][1] starts at xform 0's modWeights[1]");
    check(table->item(1, 2)->text().toDouble() == 0.75, "cell [1][2] starts at xform 1's modWeights[2]");
    check(table->item(2, 0)->text().toDouble() == 0.5, "cell [2][0] starts at xform 2's modWeights[0]");
    check(table->verticalHeaderItem(0)->background().color() == table->horizontalHeaderItem(0)->background().color(),
          "row 0's header color matches column 0's header color (both identify xform 0)");
    check(table->verticalHeaderItem(0)->background().color() != table->verticalHeaderItem(1)->background().color(),
          "different xforms get visibly different header colors");

    delete dialog;
}

void testEditingCellUpdatesFlameAndEmitsSignal() {
    auto flame = makeThreeXformFlame();
    auto* dialog = new apo::ui::XaosDialog(flame);
    auto* table = dialog->findChild<QTableWidget*>("xaosTable");
    if (!check(table != nullptr, "xaosTable found")) {
        delete dialog;
        return;
    }

    QSignalSpy spy(dialog, &apo::ui::XaosDialog::flameChanged);
    table->item(0, 2)->setText("3.5");

    check(approxEqual(flame->xform[0]->modWeights[2], 3.5), "editing a cell's text updates the corresponding modWeights entry");
    check(spy.count() == 1, "flameChanged fires exactly once per committed cell edit");

    delete dialog;
}

void testNegativeInputIsClampedToZero() {
    auto flame = makeThreeXformFlame();
    auto* dialog = new apo::ui::XaosDialog(flame);
    auto* table = dialog->findChild<QTableWidget*>("xaosTable");
    if (!check(table != nullptr, "xaosTable found")) {
        delete dialog;
        return;
    }

    table->item(0, 1)->setText("-2");

    check(approxEqual(flame->xform[0]->modWeights[1], 0.0),
          "a negative weight is clamped to 0 - buildPropTable's cumulative-sum selection assumes non-negative "
          "weights");
    check(table->item(0, 1)->text().toDouble() == 0.0, "the displayed cell text reflects the clamped value");

    delete dialog;
}

void testInvalidInputRevertsWithoutChangingFlame() {
    auto flame = makeThreeXformFlame();
    auto* dialog = new apo::ui::XaosDialog(flame);
    auto* table = dialog->findChild<QTableWidget*>("xaosTable");
    if (!check(table != nullptr, "xaosTable found")) {
        delete dialog;
        return;
    }

    QSignalSpy spy(dialog, &apo::ui::XaosDialog::flameChanged);
    table->item(0, 1)->setText("not a number");

    check(approxEqual(flame->xform[0]->modWeights[1], 0.25), "non-numeric input leaves the underlying weight unchanged");
    check(table->item(0, 1)->text().toDouble() == 0.25, "non-numeric input reverts the displayed cell text");
    check(spy.count() == 0, "flameChanged does not fire for a rejected (non-numeric) edit");

    delete dialog;
}

void testClearAllAndSetAllApplyToWholeMatrix() {
    auto flame = makeThreeXformFlame();
    auto* dialog = new apo::ui::XaosDialog(flame);
    auto* table = dialog->findChild<QTableWidget*>("xaosTable");
    auto* clearAllButton = dialog->findChild<QPushButton*>("clearAllButton");
    auto* setAllButton = dialog->findChild<QPushButton*>("setAllButton");
    if (!check(table && clearAllButton && setAllButton, "table/clearAllButton/setAllButton found")) {
        delete dialog;
        return;
    }

    QTest::mouseClick(clearAllButton, Qt::LeftButton);
    bool allZero = true;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) allZero &= approxEqual(flame->xform[i]->modWeights[j], 0.0);
    check(allZero, "Clear All sets every weight in the matrix (not just one row) to 0");
    check(table->item(1, 2)->text().toDouble() == 0.0, "Clear All refreshes the displayed table too");

    QTest::mouseClick(setAllButton, Qt::LeftButton);
    bool allOne = true;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) allOne &= approxEqual(flame->xform[i]->modWeights[j], 1.0);
    check(allOne, "Set All sets every weight in the matrix to 1");

    delete dialog;
}

void testRefreshFromFlamePicksUpStructuralEdit() {
    auto flame = makeThreeXformFlame();
    auto* dialog = new apo::ui::XaosDialog(flame);
    auto* table = dialog->findChild<QTableWidget*>("xaosTable");
    if (!check(table != nullptr, "xaosTable found")) {
        delete dialog;
        return;
    }
    check(table->rowCount() == 3, "table starts at 3x3 for a 3-xform flame");

    // Simulate an external structural edit (what EditorWindow::onAddXform
    // does to the shared flame) made while this dialog is still open.
    const int idx = flame->addXForm();
    check(idx == 3, "addXForm appended a 4th xform on the shared flame, out from under the still-open dialog");

    dialog->refreshFromFlame();

    check(table->rowCount() == 4 && table->columnCount() == 4,
          "refreshFromFlame() picks up the new xform count without needing the dialog to be reopened");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testTableStartsPopulatedFromFlame();
    testEditingCellUpdatesFlameAndEmitsSignal();
    testNegativeInputIsClampedToZero();
    testInvalidInputRevertsWithoutChangingFlame();
    testClearAllAndSetAllApplyToWholeMatrix();
    testRefreshFromFlamePicksUpStructuralEdit();

    return apo_test::reportAndExit();
}
