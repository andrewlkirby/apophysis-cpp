// Real-widget interaction tests for TransformPanel (src/ui/TransformPanel.h)
// - the underlying data model (XForm::density/transformName/variation()/
// setVariation(), VariationRegistry's name table) is already covered
// independently in core_smoke_test.cpp/variation_roundtrip_test.cpp; this
// file verifies the Qt wiring on top of it: the header fields (Name/
// Weight) and the Variations table are seeded from the selected xform,
// editing them reaches the actual XForm, weight can never reach exactly 0
// (Flame::numXForms()'s packing sentinel), the search/hide-unused filters
// work, Clear zeroes every variation, and switching the selected xform
// re-seeds every displayed value. Mirrors curves_dialog_interaction_test.cpp/
// xaos_dialog_interaction_test.cpp's approach.

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>

#include "../TestHelpers.h"
#include "SliderSpin.h"
#include "TransformPanel.h"
#include "core/VariationRegistry.h"
#include "core/edit/TriangleGeometry.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTwoXformFlame() {
    auto flame = std::make_shared<apo::Flame>();
    for (int i = 0; i < 2; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
    }
    flame->xform[0]->transformName = "Alpha";
    flame->xform[1]->transformName = "Beta";
    // linear is variation index 0 (see VariationRegistry::localVarNames()) -
    // every xform defaults to linear=1 via clear(), including xform[1]
    // here; turn it off there and give it a second, distinguishable
    // nonzero variation instead, so xform[0] (pure linear) and xform[1]
    // (pure spiral) are easy to tell apart in every test below.
    flame->xform[1]->setVariation(0, 0.0);
    const int spiralIndex = apo::VariationRegistry::instance().variationIndex("spiral");
    flame->xform[1]->setVariation(spiralIndex, 0.5);
    return flame;
}

int findRowByName(QTableWidget* table, const QString& name) {
    for (int i = 0; i < table->rowCount(); ++i) {
        if (table->item(i, 0)->text() == name) return i;
    }
    return -1;
}

// Types a value into a SliderSpin's spin box and presses Enter - the real
// end-user path (see adjust_dialog_interaction_test.cpp's identical
// helper), exercising QDoubleSpinBox::valueChanged (live) then
// QAbstractSpinBox's real Return-key handling (editingFinished).
void typeIntoSliderSpin(apo::ui::SliderSpin* ctrl, double value) {
    auto* spin = ctrl->findChild<QDoubleSpinBox*>();
    spin->setFocus();
    spin->setValue(value);
    QTest::keyClick(spin, Qt::Key_Return);
}

void testHeaderFieldsSeededFromSelectedXform() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* nameEdit = panel->findChild<QLineEdit*>("transformNameEdit");
    auto* weightSpin = panel->findChild<QDoubleSpinBox*>("transformWeightSpin");
    if (!check(nameEdit && weightSpin, "TransformPanel exposes transformNameEdit and transformWeightSpin")) {
        delete panel;
        return;
    }

    check(nameEdit->text() == "Alpha", "name edit is seeded from the selected xform's transformName");
    check(approxEqual(weightSpin->value(), 1.0), "weight spin is seeded from the selected xform's density");
    check(panel->isEnabled(), "the panel is enabled once a real xform is selected");

    delete panel;
}

void testPanelDisabledWithNoSelection() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(-1);

    check(!panel->isEnabled(), "the panel is disabled when nothing is selected");

    delete panel;
}

void testEditingWeightReachesTheXForm() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* weightSpin = panel->findChild<QDoubleSpinBox*>("transformWeightSpin");
    if (!check(weightSpin != nullptr, "transformWeightSpin found")) {
        delete panel;
        return;
    }

    QSignalSpy propertySpy(panel, &apo::ui::TransformPanel::propertyEdited);
    QSignalSpy startedSpy(panel, &apo::ui::TransformPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);

    weightSpin->setValue(3.5); // real API call, same as a user's spin-box interaction - emits valueChanged
    check(approxEqual(flame->xform[0]->density, 3.5), "editing the weight spin box updates XForm::density");
    check(propertySpy.count() >= 1, "propertyEdited fires while the weight is being edited");
    check(startedSpy.count() == 1, "editingStarted fires exactly once for a single weight-edit gesture");

    weightSpin->setFocus();
    QTest::keyClick(weightSpin, Qt::Key_Return); // real key event -> emits editingFinished
    check(finishedSpy.count() == 1, "editingFinished fires once the weight edit gesture completes");

    delete panel;
}

void testWeightNeverReachesExactlyZero() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* weightSpin = panel->findChild<QDoubleSpinBox*>("transformWeightSpin");
    if (!check(weightSpin != nullptr, "transformWeightSpin found")) {
        delete panel;
        return;
    }

    check(weightSpin->minimum() > 0.0, "the weight spin box's minimum is strictly positive");
    weightSpin->setValue(weightSpin->minimum());
    check(flame->xform[0]->density > 0.0,
          "the weight can never reach exactly 0 - Flame::numXForms() treats density==0 as its packed-array "
          "end-of-list sentinel, so 0 would silently make this (and every later) xform disappear");

    delete panel;
}

void testEditingNameReachesTheXForm() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1);

    auto* nameEdit = panel->findChild<QLineEdit*>("transformNameEdit");
    if (!check(nameEdit != nullptr, "transformNameEdit found")) {
        delete panel;
        return;
    }

    QSignalSpy startedSpy(panel, &apo::ui::TransformPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);

    QTest::keyClicks(nameEdit, " Renamed");
    check(flame->xform[1]->transformName == "Beta Renamed", "typing into the name edit updates XForm::transformName");
    check(startedSpy.count() == 1, "editingStarted fires exactly once for the whole typing gesture");

    QTest::keyClick(nameEdit, Qt::Key_Return);
    check(finishedSpy.count() == 1, "editingFinished fires once Return commits the name edit");

    delete panel;
}

void testVariationsTableSeededAndEditable() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1);

    auto* table = panel->findChild<QTableWidget*>("variationsTable");
    if (!check(table != nullptr, "variationsTable found")) {
        delete panel;
        return;
    }
    check(table->rowCount() == apo::VariationRegistry::instance().nrVar(),
          "the table has exactly one row per registered variation");

    const int linearRow = findRowByName(table, "linear");
    const int spiralRow = findRowByName(table, "spiral");
    if (!check(linearRow >= 0 && spiralRow >= 0, "linear and spiral rows found")) {
        delete panel;
        return;
    }
    check(table->item(linearRow, 1)->text().toDouble() == 0.0, "xform 1's linear weight is seeded as 0");
    check(approxEqual(table->item(spiralRow, 1)->text().toDouble(), 0.5), "xform 1's spiral weight is seeded as 0.5");

    QSignalSpy propertySpy(panel, &apo::ui::TransformPanel::propertyEdited);
    table->item(linearRow, 1)->setText("0.75");
    check(approxEqual(flame->xform[1]->variation(0), 0.75), "editing a variation cell updates XForm::setVariation()");
    check(propertySpy.count() == 1, "propertyEdited fires exactly once for a committed cell edit");

    delete panel;
}

void testInvalidVariationInputReverts() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1);

    auto* table = panel->findChild<QTableWidget*>("variationsTable");
    if (!check(table != nullptr, "variationsTable found")) {
        delete panel;
        return;
    }
    const int spiralRow = findRowByName(table, "spiral");
    if (!check(spiralRow >= 0, "spiral row found")) {
        delete panel;
        return;
    }

    table->item(spiralRow, 1)->setText("not a number");
    check(approxEqual(flame->xform[1]->variation(apo::VariationRegistry::instance().variationIndex("spiral")), 0.5),
          "non-numeric input leaves the underlying variation weight unchanged");
    check(table->item(spiralRow, 1)->text().toDouble() == 0.5, "non-numeric input reverts the displayed cell text");

    delete panel;
}

void testSearchFilterHidesNonMatchingRows() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* table = panel->findChild<QTableWidget*>("variationsTable");
    auto* search = panel->findChild<QLineEdit*>("variationSearchEdit");
    if (!check(table && search, "variationsTable and variationSearchEdit found")) {
        delete panel;
        return;
    }

    search->setText("spiral");
    const int spiralRow = findRowByName(table, "spiral");
    const int linearRow = findRowByName(table, "linear");
    if (!check(spiralRow >= 0 && linearRow >= 0, "rows found")) {
        delete panel;
        return;
    }
    check(!table->isRowHidden(spiralRow), "a row matching the search text stays visible");
    check(table->isRowHidden(linearRow), "a row not matching the search text is hidden");

    search->clear();
    check(!table->isRowHidden(linearRow), "clearing the search shows every row again");

    delete panel;
}

void testHideUnusedChecksOnlyNonZeroWeights() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1); // linear=0, spiral=0.5

    auto* table = panel->findChild<QTableWidget*>("variationsTable");
    auto* hideUnused = panel->findChild<QCheckBox*>("hideUnusedVariationsCheck");
    if (!check(table && hideUnused, "variationsTable and hideUnusedVariationsCheck found")) {
        delete panel;
        return;
    }

    hideUnused->setChecked(true);
    const int spiralRow = findRowByName(table, "spiral");
    const int linearRow = findRowByName(table, "linear");
    if (!check(spiralRow >= 0 && linearRow >= 0, "rows found")) {
        delete panel;
        return;
    }
    check(!table->isRowHidden(spiralRow), "a nonzero-weight variation stays visible when hiding unused ones");
    check(table->isRowHidden(linearRow), "a zero-weight variation is hidden when hiding unused ones");

    delete panel;
}

void testClearButtonZeroesEveryVariation() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1); // has spiral=0.5 set
    // Unlike every other control tested in this file (which target a
    // specific child widget directly via setValue()/keyClicks() -
    // reaching it regardless of real screen geometry), QTest::mouseClick
    // is *position*-based: it needs the button to have real, laid-out
    // geometry to land on, which a never-shown top-level QWidget (unlike
    // a QDialog, which several other dialogs in this port click buttons
    // on unshown) doesn't reliably have on the offscreen QPA platform.
    panel->show();

    auto* clearButton = panel->findChild<QPushButton*>("clearVariationsButton");
    if (!check(clearButton != nullptr, "clearVariationsButton found")) {
        delete panel;
        return;
    }

    QSignalSpy startedSpy(panel, &apo::ui::TransformPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);
    QTest::mouseClick(clearButton, Qt::LeftButton);

    bool allZero = true;
    for (int i = 0; i < flame->xform[1]->numVariations(); ++i) {
        if (flame->xform[1]->variation(i) != 0.0) allZero = false;
    }
    check(allZero, "Clear sets every one of the selected xform's variation weights to 0");
    check(startedSpy.count() == 1 && finishedSpy.count() == 1, "Clear brackets its edit in exactly one undo-worthy gesture");

    delete panel;
}

void testColorsTabSeededFromSelectedXform() {
    auto flame = makeTwoXformFlame();
    flame->xform[0]->color = 0.25;
    flame->xform[0]->symmetry = -0.5;
    flame->xform[0]->transOpacity = 0.6;
    flame->xform[0]->pluginColor = 0.1;
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* colorSpin = panel->findChild<apo::ui::SliderSpin*>("colorSpin");
    auto* colorSpeedSpin = panel->findChild<apo::ui::SliderSpin*>("colorSpeedSpin");
    auto* opacitySpin = panel->findChild<apo::ui::SliderSpin*>("opacitySpin");
    auto* directColorSpin = panel->findChild<apo::ui::SliderSpin*>("directColorSpin");
    if (!check(colorSpin && colorSpeedSpin && opacitySpin && directColorSpin,
                "TransformPanel exposes colorSpin/colorSpeedSpin/opacitySpin/directColorSpin")) {
        delete panel;
        return;
    }

    check(approxEqual(colorSpin->value(), 0.25), "colorSpin is seeded from XForm::color");
    check(approxEqual(colorSpeedSpin->value(), -0.5), "colorSpeedSpin is seeded from XForm::symmetry (color_speed)");
    check(approxEqual(opacitySpin->value(), 0.6), "opacitySpin is seeded from XForm::transOpacity");
    check(approxEqual(directColorSpin->value(), 0.1), "directColorSpin is seeded from XForm::pluginColor");

    delete panel;
}

void testEditingColorFieldsReachesTheXForm() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* colorSpin = panel->findChild<apo::ui::SliderSpin*>("colorSpin");
    auto* colorSpeedSpin = panel->findChild<apo::ui::SliderSpin*>("colorSpeedSpin");
    auto* opacitySpin = panel->findChild<apo::ui::SliderSpin*>("opacitySpin");
    auto* directColorSpin = panel->findChild<apo::ui::SliderSpin*>("directColorSpin");
    if (!check(colorSpin && colorSpeedSpin && opacitySpin && directColorSpin, "color controls found")) {
        delete panel;
        return;
    }

    QSignalSpy propertySpy(panel, &apo::ui::TransformPanel::propertyEdited);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);

    typeIntoSliderSpin(colorSpin, 0.75);
    check(approxEqual(flame->xform[0]->color, 0.75), "editing colorSpin updates XForm::color");

    typeIntoSliderSpin(colorSpeedSpin, -0.9);
    check(approxEqual(flame->xform[0]->symmetry, -0.9), "editing colorSpeedSpin updates XForm::symmetry");

    typeIntoSliderSpin(opacitySpin, 0.2);
    check(approxEqual(flame->xform[0]->transOpacity, 0.2), "editing opacitySpin updates XForm::transOpacity");

    typeIntoSliderSpin(directColorSpin, 0.4);
    check(approxEqual(flame->xform[0]->pluginColor, 0.4), "editing directColorSpin updates XForm::pluginColor");

    check(propertySpy.count() >= 4, "propertyEdited fires for each of the four edits");
    check(finishedSpy.count() == 4, "editingFinished fires once per committed field edit");

    delete panel;
}

void testSoloCheckboxTogglesFlameSoloXform() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1);

    auto* soloCheck = panel->findChild<QCheckBox*>("soloCheck");
    if (!check(soloCheck != nullptr, "soloCheck found")) {
        delete panel;
        return;
    }
    check(!soloCheck->isChecked(), "solo starts unchecked - flame->soloXform defaults to -1");

    QSignalSpy startedSpy(panel, &apo::ui::TransformPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);
    soloCheck->setChecked(true);
    check(flame->soloXform == 1, "checking Solo sets Flame::soloXform to the selected xform's index");
    check(startedSpy.count() == 1 && finishedSpy.count() == 1, "toggling Solo brackets one atomic undo-worthy gesture");

    soloCheck->setChecked(false);
    check(flame->soloXform == -1, "unchecking Solo clears Flame::soloXform back to -1");

    delete panel;
}

void testVariablesTableTracksOwningVariationWeight() {
    auto flame = makeTwoXformFlame();
    const int curlIndex = apo::VariationRegistry::instance().variationIndex("curl");
    flame->xform[0]->setVariation(curlIndex, 0.0); // xform 0 starts with curl unused
    double c1 = 0.3;
    flame->xform[0]->setVariable("curl_c1", c1);

    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* table = panel->findChild<QTableWidget*>("variablesTable");
    if (!check(table != nullptr, "variablesTable found")) {
        delete panel;
        return;
    }
    check(table->rowCount() == apo::VariationRegistry::instance().numVariableNames(),
          "the table has exactly one row per registered variable");

    const int c1Row = findRowByName(table, "curl_c1");
    if (!check(c1Row >= 0, "curl_c1 row found")) {
        delete panel;
        return;
    }
    check(table->isRowHidden(c1Row), "a variable belonging to an unused (zero-weight) variation starts hidden");

    QTableWidget* variationsTable = panel->findChild<QTableWidget*>("variationsTable");
    const int curlRow = findRowByName(variationsTable, "curl");
    if (!check(curlRow >= 0, "curl row found in variationsTable")) {
        delete panel;
        return;
    }
    variationsTable->item(curlRow, 1)->setText("0.4"); // gives xform 0 a nonzero curl weight
    check(!table->isRowHidden(c1Row), "giving the owning variation a nonzero weight reveals its variable row");
    check(approxEqual(table->item(c1Row, 1)->text().toDouble(), 0.3), "the revealed row shows the variable's current value");

    QSignalSpy propertySpy(panel, &apo::ui::TransformPanel::propertyEdited);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);
    table->item(c1Row, 1)->setText("0.9");
    double readBack = 0.0;
    flame->xform[0]->getVariable("curl_c1", readBack);
    check(approxEqual(readBack, 0.9), "editing a variables-table cell updates the variation's own parameter");
    check(propertySpy.count() == 1 && finishedSpy.count() == 1, "the variable edit fires exactly one edit gesture");

    variationsTable->item(curlRow, 1)->setText("0");
    check(table->isRowHidden(c1Row), "dropping the owning variation's weight back to 0 hides its variable row again");

    delete panel;
}

void testTransformTabSeededFromSelectedXform() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0); // pure linear -> identity affine coefs

    auto* oX = panel->findChild<QDoubleSpinBox*>("vertexOX");
    auto* oY = panel->findChild<QDoubleSpinBox*>("vertexOY");
    auto* xX = panel->findChild<QDoubleSpinBox*>("vertexXX");
    auto* xY = panel->findChild<QDoubleSpinBox*>("vertexXY");
    auto* yX = panel->findChild<QDoubleSpinBox*>("vertexYX");
    auto* yY = panel->findChild<QDoubleSpinBox*>("vertexYY");
    if (!check(oX && oY && xX && xY && yX && yY, "TransformPanel exposes the six vertex spin boxes")) {
        delete panel;
        return;
    }

    check(approxEqual(oX->value(), 0.0) && approxEqual(oY->value(), 0.0), "O is seeded as (0,0) for an identity xform");
    check(approxEqual(xX->value(), 1.0) && approxEqual(xY->value(), 0.0), "X is seeded as (1,0) for an identity xform");
    check(approxEqual(yX->value(), 0.0) && approxEqual(yY->value(), 1.0), "Y is seeded as (0,1) for an identity xform");

    delete panel;
}

void testEditingVertexFieldsReachesTheXForm() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* xX = panel->findChild<QDoubleSpinBox*>("vertexXX");
    if (!check(xX != nullptr, "vertexXX found")) {
        delete panel;
        return;
    }

    QSignalSpy propertySpy(panel, &apo::ui::TransformPanel::propertyEdited);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);

    xX->setValue(2.5);
    const apo::Triangle t = apo::triangleFromCoefs(flame->xform[0]->c);
    check(approxEqual(t.x.x, 2.5), "editing the X vertex's X field updates XForm::c via coefsFromTriangle");
    check(propertySpy.count() >= 1, "propertyEdited fires while a vertex field is being edited");

    xX->setFocus();
    QTest::keyClick(xX, Qt::Key_Return);
    check(finishedSpy.count() == 1, "editingFinished fires once the vertex edit gesture completes");

    delete panel;
}

void testEditPostToggleSwitchesActiveMatrixAndEmitsSignal() {
    auto flame = makeTwoXformFlame();
    flame->xform[0]->p = {{{2, 0}, {0, 2}, {5, 5}}}; // a distinguishable post-transform
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* editPostCheck = panel->findChild<QCheckBox*>("editPostCheck");
    auto* oX = panel->findChild<QDoubleSpinBox*>("vertexOX");
    if (!check(editPostCheck && oX, "editPostCheck and vertexOX found")) {
        delete panel;
        return;
    }
    check(!panel->editingPostTransform(), "starts editing the main transform");

    QSignalSpy toggleSpy(panel, &apo::ui::TransformPanel::editingPostTransformChanged);
    editPostCheck->setChecked(true);
    check(panel->editingPostTransform(), "checking the box switches to the post transform");
    check(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool(),
          "editingPostTransformChanged(true) fires exactly once");

    const apo::Triangle postTriangle = apo::triangleFromCoefs(flame->xform[0]->p);
    check(approxEqual(oX->value(), postTriangle.o.x), "vertex fields now reflect XForm::p, not XForm::c");

    delete panel;
}

void testSetEditingPostTransformIsProgrammaticAndSilent() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    QSignalSpy toggleSpy(panel, &apo::ui::TransformPanel::editingPostTransformChanged);
    panel->setEditingPostTransform(true);
    check(panel->editingPostTransform(), "setEditingPostTransform updates the panel's state");
    check(toggleSpy.count() == 0,
          "setEditingPostTransform is a programmatic sync - it does not re-emit editingPostTransformChanged");

    auto* editPostCheck = panel->findChild<QCheckBox*>("editPostCheck");
    if (check(editPostCheck != nullptr, "editPostCheck found")) {
        check(editPostCheck->isChecked(), "the checkbox itself reflects the programmatic change");
    }

    delete panel;
}

void testRotate90CWThenCCWRestoresTheOriginalTriangle() {
    auto flame = makeTwoXformFlame();
    flame->xform[1]->c = {{{1, 0.3}, {0.2, 1.1}, {0.7, -0.4}}}; // a non-trivial, distinguishable matrix
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1);
    panel->show(); // position-based QTest::mouseClick needs real geometry - see testClearButtonZeroesEveryVariation

    auto* cwButton = panel->findChild<QPushButton*>("rotate90CWButton");
    auto* ccwButton = panel->findChild<QPushButton*>("rotate90CCWButton");
    if (!check(cwButton && ccwButton, "rotate90CWButton and rotate90CCWButton found")) {
        delete panel;
        return;
    }

    const auto originalC = flame->xform[1]->c;
    QSignalSpy startedSpy(panel, &apo::ui::TransformPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);
    QTest::mouseClick(cwButton, Qt::LeftButton);
    QTest::mouseClick(ccwButton, Qt::LeftButton);

    bool restored = true;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (!approxEqual(flame->xform[1]->c[i][j], originalC[i][j])) restored = false;
        }
    }
    check(restored, "rotating 90 CW then 90 CCW restores the original triangle");
    check(startedSpy.count() == 2 && finishedSpy.count() == 2, "each rotate-90 click brackets its own undo-worthy gesture");

    delete panel;
}

void testFlipHorizontalTwiceRestoresTheOriginalTriangle() {
    auto flame = makeTwoXformFlame();
    flame->xform[1]->c = {{{1, 0.3}, {0.2, 1.1}, {0.7, -0.4}}};
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(1);
    panel->show();

    auto* flipButton = panel->findChild<QPushButton*>("flipHorizontalButton");
    if (!check(flipButton != nullptr, "flipHorizontalButton found")) {
        delete panel;
        return;
    }

    const auto originalC = flame->xform[1]->c;
    QTest::mouseClick(flipButton, Qt::LeftButton);
    check(flame->xform[1]->c != originalC, "flipping horizontally once changes the triangle");
    QTest::mouseClick(flipButton, Qt::LeftButton);

    bool restored = true;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (!approxEqual(flame->xform[1]->c[i][j], originalC[i][j])) restored = false;
        }
    }
    check(restored, "flipping horizontally twice is an involution - it restores the original triangle");

    delete panel;
}

void testRotateByAndScaleByButtonsMutateTheActiveMatrixAndBracketOneGesture() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);
    panel->show();

    auto* rotateBySpin = panel->findChild<QDoubleSpinBox*>("rotateBySpin");
    auto* rotateButton = panel->findChild<QPushButton*>("rotateByButton");
    auto* scaleBySpin = panel->findChild<QDoubleSpinBox*>("scaleBySpin");
    auto* scaleButton = panel->findChild<QPushButton*>("scaleByButton");
    if (!check(rotateBySpin && rotateButton && scaleBySpin && scaleButton, "rotate/scale controls found")) {
        delete panel;
        return;
    }

    QSignalSpy startedSpy(panel, &apo::ui::TransformPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::TransformPanel::editingFinished);

    const auto originalC = flame->xform[0]->c;
    rotateBySpin->setValue(90.0);
    QTest::mouseClick(rotateButton, Qt::LeftButton);
    check(flame->xform[0]->c != originalC, "clicking Rotate changes the active matrix");

    const auto afterRotateC = flame->xform[0]->c;
    const apo::Triangle beforeScale = apo::triangleFromCoefs(afterRotateC);
    const apo::Point2 pivotBeforeScale = apo::centroid(beforeScale);
    const double distBeforeScale = apo::distance(beforeScale.x, pivotBeforeScale);

    scaleBySpin->setValue(100.0); // scale by +100% -> factor 2.0
    QTest::mouseClick(scaleButton, Qt::LeftButton);
    check(flame->xform[0]->c != afterRotateC, "clicking Scale changes the active matrix");

    const apo::Triangle afterScale = apo::triangleFromCoefs(flame->xform[0]->c);
    const apo::Point2 pivotAfterScale = apo::centroid(afterScale);
    const double distAfterScale = apo::distance(afterScale.x, pivotAfterScale);
    check(approxEqual(distAfterScale, distBeforeScale * 2.0),
          "scaling by +100% doubles each vertex's distance from the triangle's own centroid");

    check(startedSpy.count() == 2 && finishedSpy.count() == 2, "each button click brackets exactly one undo-worthy gesture");

    delete panel;
}

void testRefreshTransformTabFromCanvasPicksUpExternalMutation() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* xX = panel->findChild<QDoubleSpinBox*>("vertexXX");
    if (!check(xX != nullptr, "vertexXX found")) {
        delete panel;
        return;
    }
    check(approxEqual(xX->value(), 1.0), "starts at the identity xform's X vertex");

    // Simulate what a TriangleCanvas drag does: mutate XForm::c directly,
    // bypassing TransformPanel entirely.
    flame->xform[0]->c[0][0] = 3.0;
    check(approxEqual(xX->value(), 1.0), "the field doesn't auto-poll - it's still stale before an explicit refresh");

    panel->refreshTransformTabFromCanvas();
    const apo::Triangle t = apo::triangleFromCoefs(flame->xform[0]->c);
    check(approxEqual(xX->value(), t.x.x),
          "refreshTransformTabFromCanvas() re-seeds the vertex fields from the externally-mutated XForm::c");

    delete panel;
}

void testFinalXformIsSelectableAndEditableLikeAnyOtherXform() {
    auto flame = makeTwoXformFlame();
    flame->finalXformEnabled = true;
    flame->finalXform->clear();
    flame->finalXform->transformName = "FinalOne";
    flame->finalXform->density = 1.0;

    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(apo::kFinalXformIndex);

    auto* nameEdit = panel->findChild<QLineEdit*>("transformNameEdit");
    if (!check(nameEdit != nullptr, "transformNameEdit found")) {
        delete panel;
        return;
    }
    check(panel->isEnabled(), "the panel is enabled when the final transform is selected");
    check(nameEdit->text() == "FinalOne", "the header fields are seeded from Flame::finalXform, not Flame::xform[]");

    QTest::keyClicks(nameEdit, " Renamed");
    check(flame->finalXform->transformName == "FinalOne Renamed", "editing the name field reaches Flame::finalXform");
    check(flame->xform[0]->transformName != "FinalOne Renamed" && flame->xform[1]->transformName != "FinalOne Renamed",
          "editing the final transform's name leaves every regular xform's name untouched");

    delete panel;
}

void testFinalXformSelectionDisabledWhenNotEnabled() {
    auto flame = makeTwoXformFlame();
    flame->finalXformEnabled = false;

    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(apo::kFinalXformIndex);

    check(!panel->isEnabled(),
          "selecting kFinalXformIndex while Flame::finalXformEnabled is false shows disabled/blank fields, "
          "same as selecting -1");

    delete panel;
}

void testSwitchingSelectionReseedsEveryField() {
    auto flame = makeTwoXformFlame();
    auto* panel = new apo::ui::TransformPanel();
    panel->setFlame(flame);
    panel->setSelectedXform(0);

    auto* nameEdit = panel->findChild<QLineEdit*>("transformNameEdit");
    auto* table = panel->findChild<QTableWidget*>("variationsTable");
    if (!check(nameEdit && table, "controls found")) {
        delete panel;
        return;
    }
    check(nameEdit->text() == "Alpha", "starts showing xform 0's name");

    panel->setSelectedXform(1);
    check(nameEdit->text() == "Beta", "switching selection re-seeds the name field");
    const int spiralRow = findRowByName(table, "spiral");
    if (check(spiralRow >= 0, "spiral row found")) {
        check(approxEqual(table->item(spiralRow, 1)->text().toDouble(), 0.5),
              "switching selection re-seeds the variations table");
    }

    delete panel;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testHeaderFieldsSeededFromSelectedXform();
    testPanelDisabledWithNoSelection();
    testEditingWeightReachesTheXForm();
    testWeightNeverReachesExactlyZero();
    testEditingNameReachesTheXForm();
    testVariationsTableSeededAndEditable();
    testInvalidVariationInputReverts();
    testSearchFilterHidesNonMatchingRows();
    testHideUnusedChecksOnlyNonZeroWeights();
    testClearButtonZeroesEveryVariation();
    testColorsTabSeededFromSelectedXform();
    testEditingColorFieldsReachesTheXForm();
    testSoloCheckboxTogglesFlameSoloXform();
    testVariablesTableTracksOwningVariationWeight();
    testTransformTabSeededFromSelectedXform();
    testEditingVertexFieldsReachesTheXForm();
    testEditPostToggleSwitchesActiveMatrixAndEmitsSignal();
    testSetEditingPostTransformIsProgrammaticAndSilent();
    testRotate90CWThenCCWRestoresTheOriginalTriangle();
    testFlipHorizontalTwiceRestoresTheOriginalTriangle();
    testRotateByAndScaleByButtonsMutateTheActiveMatrixAndBracketOneGesture();
    testRefreshTransformTabFromCanvasPicksUpExternalMutation();
    testFinalXformIsSelectableAndEditableLikeAnyOtherXform();
    testFinalXformSelectionDisabledWhenNotEnabled();
    testSwitchingSelectionReseedsEveryField();

    return apo_test::reportAndExit();
}
