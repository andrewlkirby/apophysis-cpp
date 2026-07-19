#include "TransformPanel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "SliderSpin.h"
#include "core/VariationRegistry.h"

namespace apo::ui {

namespace {
// XForm::density == 0 is Flame::numXForms()'s packed-array end-of-list
// sentinel (see Flame.h) - a weight editor that let a user type exactly 0
// would silently make this xform (and every one after it) disappear from
// numXForms(), corrupting the flame far worse than any UI bug should.
// Clamp to a small positive floor instead, exactly as the original's own
// weight field does.
constexpr double kMinWeight = 0.001;

constexpr double kPi = 3.14159265358979323846;

QString formatValue(double v) { return QString::number(v, 'g', 6); }

// One-line summaries for the Variations tab's optional Description column,
// keyed by VariationRegistry::varName() (the 29 local variations plus every
// built-in registered/plugin variation). A name with no entry here (e.g. a
// third-party plugin loaded at runtime) just shows a blank description
// rather than an error - this table is a convenience, not a completeness
// guarantee.
const QHash<QString, QString>& variationDescriptions() {
    static const QHash<QString, QString> descriptions = {
        // Local variations (XFormMan.pas's cvarnames order).
        {"linear", "Passes coordinates through unchanged."},
        {"flatten", "Collapses the Z coordinate to zero."},
        {"sinusoidal", "Applies sine to each coordinate, producing wavy folds."},
        {"spherical", "Divides by radius squared, turning lines into circles."},
        {"swirl", "Rotates points by an amount proportional to distance from origin."},
        {"horseshoe", "Reflects points around the X axis in polar form."},
        {"polar", "Remaps coordinates to angle/radius (theta, r)."},
        {"disc", "Maps angle and radius into a spiral disc shape."},
        {"spiral", "Wraps points around the origin in a logarithmic spiral."},
        {"hyperbolic", "Combines polar angle and inverse radius into a hyperbolic curve."},
        {"diamond", "Produces a diamond-shaped warp from angle and radius."},
        {"eyefish", "Fisheye-lens style radial distortion."},
        {"bubble", "Projects points onto a sphere for a bubble-like bulge."},
        {"cylinder", "Wraps the X coordinate onto a cylinder."},
        {"noise", "Adds random jitter scaled by a random radius."},
        {"blur", "Scatters points randomly within a disc."},
        {"gaussian_blur", "Scatters points using a Gaussian-distributed random offset."},
        {"zblur", "Adds random jitter to the Z coordinate only."},
        {"blur3D", "Scatters points randomly in X, Y, and Z."},
        {"pre_blur", "Applies blur's random scatter before the rest of the transform."},
        {"pre_zscale", "Scales Z before the transform's other variations run."},
        {"pre_ztranslate", "Offsets Z before the transform's other variations run."},
        {"pre_rotate_x", "Rotates around the X axis before the transform's other variations run."},
        {"pre_rotate_y", "Rotates around the Y axis before the transform's other variations run."},
        {"zscale", "Scales the Z coordinate by a fixed factor."},
        {"ztranslate", "Offsets the Z coordinate by a fixed amount."},
        {"zcone", "Adds to Z based on radius, forming a cone shape."},
        {"post_rotate_x", "Rotates around the X axis after the transform's other variations run."},
        {"post_rotate_y", "Rotates around the Y axis after the transform's other variations run."},
        // Registered variations.
        {"auger", "Twists points along X and Y with a periodic wave."},
        {"bipolar", "Bipolar-coordinate warp that folds space around two poles."},
        {"blur_circle", "Scatters points evenly across a filled circle."},
        {"blur_zoom", "Blurs points with a zoom streak centered on a movable point."},
        {"blur_pixelize", "Snaps points onto a coarse pixel grid for a blocky look."},
        {"crop", "Clips points outside a rectangle."},
        {"bwraps", "Wraps points around a circular boundary."},
        {"cross", "Folds space based on x squared minus y squared, forming an hourglass shape."},
        {"curl3D", "3D version of curl: bends space using cubic terms in X, Y, and Z."},
        {"curl", "Bends space using a complex reciprocal function."},
        {"elliptic", "Maps points onto elliptical arcs using an elliptic-integral-style formula."},
        {"epispiral", "Draws an epispiral (star/rose) curve pattern."},
        {"escher", "Complex power/log spiral fold, evoking Escher-style tessellations."},
        {"falloff2", "Randomly scatters points and color based on distance from a center point."},
        {"fan2", "Fan-shaped wedge distortion with adjustable segment width."},
        {"foci", "Hyperbolic-cosine based fold using two focal points."},
        {"hemisphere", "Projects points onto the upper half of a sphere."},
        {"julia3D", "3D Julia-set-style square-root fold with a Z power term."},
        {"julia3Dz", "julia3D variant that also folds the Z coordinate."},
        {"juliascope", "Kaleidoscopic Julia-set fold with alternating mirror symmetry."},
        {"julian", "Julia-set-style fold using an Nth root with a random branch."},
        {"log", "Takes the natural log of the squared radius, compressing outward points."},
        {"lazysusan", "Rotates points near a movable center, spiraling outward beyond a radius."},
        {"pdj", "Warp driven by four sine/cosine parameters (de Jong attractor style)."},
        {"mobius", "Applies a complex Mobius (fractional-linear) transformation."},
        {"loonie", "Inverts points inside a boundary radius outward; leaves outside points unchanged."},
        {"ngon", "Warps points toward the edges of a regular N-sided polygon."},
        {"post_bwraps", "bwraps applied after the transform's other variations."},
        {"polar2", "Alternate polar remap combining log-radius and angle with independent scaling."},
        {"post_crop", "crop applied after the transform's other variations."},
        {"post_curl", "curl applied after the transform's other variations."},
        {"post_curl3D", "curl3D applied after the transform's other variations."},
        {"post_falloff2", "falloff2 applied after the transform's other variations."},
        {"pre_crop", "crop applied before the transform's other variations."},
        {"pre_bwraps", "bwraps applied before the transform's other variations."},
        {"pre_sinusoidal", "sinusoidal applied before the transform's other variations."},
        {"rings2", "Folds radius into concentric rings of varying width."},
        {"rectangles", "Snaps points onto a repeating rectangular grid."},
        {"pre_spherical", "spherical applied before the transform's other variations."},
        {"pre_falloff2", "falloff2 applied before the transform's other variations."},
        {"pre_disc", "disc applied before the transform's other variations."},
        {"radial_blur", "Blurs points along a blend of radial (zoom) and angular (spin) directions."},
        {"scry", "Pulls points toward the origin, concentrating them into a central well."},
        {"separation", "Pushes points apart along X and Y, opening a gap through the origin."},
        {"splits", "Shifts each quadrant of points outward by a fixed X/Y offset."},
        {"waves2", "Sine-wave ripple with independent frequency and scale per axis."},
        {"wedge", "Slices space into angular wedges and rotates them, with optional swirl."},
        // Built-in plugin variations.
        {"julia", "Classic complex square-root Julia fold."},
        {"fisheye", "Fisheye-lens radial distortion (plugin variant of eyefish)."},
    };
    return descriptions;
}
} // namespace

TransformPanel::TransformPanel(QWidget* parent) : QWidget(parent) {
    // A concrete default size (like every dialog in this port sets in its
    // own constructor) so child widgets get real geometry even before
    // this panel is ever shown or added to a real parent layout - without
    // it, a synthetic click (QTest::mouseClick) on e.g. the Clear button
    // can land on a still-0x0 widget and silently miss. EditorWindow's
    // splitter overrides this the moment the panel is actually embedded.
    resize(320, 480);

    auto* rootLayout = new QVBoxLayout(this);

    auto* header = new QFormLayout();
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName("transformNameEdit");
    connect(nameEdit_, &QLineEdit::textEdited, this, &TransformPanel::onNameEdited);
    connect(nameEdit_, &QLineEdit::editingFinished, this, &TransformPanel::onNameEditingFinished);
    header->addRow("Name", nameEdit_);

    weightSpin_ = new QDoubleSpinBox(this);
    weightSpin_->setObjectName("transformWeightSpin");
    weightSpin_->setRange(kMinWeight, 1000.0);
    weightSpin_->setDecimals(6);
    connect(weightSpin_, &QDoubleSpinBox::valueChanged, this, &TransformPanel::onWeightChanged);
    connect(weightSpin_, &QDoubleSpinBox::editingFinished, this, &TransformPanel::onWeightEditingFinished);
    header->addRow("Weight", weightSpin_);
    rootLayout->addLayout(header);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(buildTransformTab(), "Transform");
    tabs_->addTab(buildColorsTab(), "Colors");
    tabs_->addTab(buildVariationsTab(), "Variations");
    tabs_->addTab(buildVariablesTab(), "Variables");
    rootLayout->addWidget(tabs_, /*stretch=*/1);

    refreshHeaderFields();
    setEnabled(false);
}

QWidget* TransformPanel::buildVariationsTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* searchRow = new QHBoxLayout();
    variationSearchEdit_ = new QLineEdit(tab);
    variationSearchEdit_->setObjectName("variationSearchEdit");
    variationSearchEdit_->setPlaceholderText("Search...");
    connect(variationSearchEdit_, &QLineEdit::textChanged, this, &TransformPanel::onVariationSearchChanged);
    searchRow->addWidget(variationSearchEdit_, /*stretch=*/1);
    layout->addLayout(searchRow);

    variationsTable_ = new QTableWidget(tab);
    variationsTable_->setObjectName("variationsTable");
    variationsTable_->setColumnCount(3);
    variationsTable_->setHorizontalHeaderLabels({"Name", "Value", "Description"});
    variationsTable_->horizontalHeader()->setStretchLastSection(true);
    variationsTable_->verticalHeader()->setVisible(false);
    // Off by default: most editing happens with the compact Name/Value pair
    // only, and the Description column's prose text would otherwise force
    // this already-narrow panel wider or into horizontal scrolling.
    variationsTable_->setColumnHidden(2, true);

    const auto& registry = apo::VariationRegistry::instance();
    const auto& descriptions = variationDescriptions();
    const int n = registry.nrVar();
    variationsTable_->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const QString name = QString::fromStdString(registry.varName(i));
        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        variationsTable_->setItem(i, 0, nameItem);
        variationsTable_->setItem(i, 1, new QTableWidgetItem());
        auto* descItem = new QTableWidgetItem(descriptions.value(name));
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        variationsTable_->setItem(i, 2, descItem);
    }
    connect(variationsTable_, &QTableWidget::itemChanged, this, &TransformPanel::onVariationCellChanged);
    layout->addWidget(variationsTable_, /*stretch=*/1);

    auto* buttonRow = new QHBoxLayout();
    hideUnusedCheck_ = new QCheckBox("Hide unused variations", tab);
    hideUnusedCheck_->setObjectName("hideUnusedVariationsCheck");
    connect(hideUnusedCheck_, &QCheckBox::toggled, this, &TransformPanel::onHideUnusedToggled);
    buttonRow->addWidget(hideUnusedCheck_);
    showDescriptionsCheck_ = new QCheckBox("Show descriptions", tab);
    showDescriptionsCheck_->setObjectName("showVariationDescriptionsCheck");
    connect(showDescriptionsCheck_, &QCheckBox::toggled, this, &TransformPanel::onShowDescriptionsToggled);
    buttonRow->addWidget(showDescriptionsCheck_);
    buttonRow->addStretch(1);
    clearVariationsButton_ = new QPushButton("Clear", tab);
    clearVariationsButton_->setObjectName("clearVariationsButton");
    connect(clearVariationsButton_, &QPushButton::clicked, this, &TransformPanel::onClearVariationsClicked);
    buttonRow->addWidget(clearVariationsButton_);
    layout->addLayout(buttonRow);

    return tab;
}

QWidget* TransformPanel::buildTransformTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    editPostCheck_ = new QCheckBox("Edit post transform", tab);
    editPostCheck_->setObjectName("editPostCheck");
    connect(editPostCheck_, &QCheckBox::toggled, this, &TransformPanel::onEditPostToggled);
    layout->addWidget(editPostCheck_);

    auto makeVertexSpin = [tab](const char* name) {
        auto* spin = new QDoubleSpinBox(tab);
        spin->setObjectName(name);
        spin->setRange(-1000.0, 1000.0);
        spin->setDecimals(6);
        return spin;
    };
    vertexOX_ = makeVertexSpin("vertexOX");
    vertexOY_ = makeVertexSpin("vertexOY");
    vertexXX_ = makeVertexSpin("vertexXX");
    vertexXY_ = makeVertexSpin("vertexXY");
    vertexYX_ = makeVertexSpin("vertexYX");
    vertexYY_ = makeVertexSpin("vertexYY");
    for (auto* spin : {vertexOX_, vertexOY_, vertexXX_, vertexXY_, vertexYX_, vertexYY_}) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, &TransformPanel::onVertexFieldChanged);
        connect(spin, &QDoubleSpinBox::editingFinished, this, &TransformPanel::onVertexFieldEditingFinished);
    }

    auto* vertexForm = new QFormLayout();
    auto* oRow = new QHBoxLayout();
    oRow->addWidget(vertexOX_);
    oRow->addWidget(vertexOY_);
    vertexForm->addRow("O (X, Y)", oRow);
    auto* xRow = new QHBoxLayout();
    xRow->addWidget(vertexXX_);
    xRow->addWidget(vertexXY_);
    vertexForm->addRow("X (X, Y)", xRow);
    auto* yRow = new QHBoxLayout();
    yRow->addWidget(vertexYX_);
    yRow->addWidget(vertexYY_);
    vertexForm->addRow("Y (X, Y)", yRow);
    layout->addLayout(vertexForm);

    // Packed two tool rows (rotate-by/scale-by, then the four exact-value
    // buttons) rather than one row per tool - this tab shares a height-
    // constrained splitter row with the canvas, and every extra row here
    // directly shrinks the canvas's vertical budget.
    auto* rotateScaleRow = new QHBoxLayout();
    rotateBySpin_ = new QDoubleSpinBox(tab);
    rotateBySpin_->setObjectName("rotateBySpin");
    rotateBySpin_->setRange(-360.0, 360.0);
    rotateBySpin_->setValue(15.0);
    rotateBySpin_->setSuffix(QString::fromUtf8("\xC2\xB0")); // degree sign
    auto* rotateButton = new QPushButton("Rotate", tab);
    rotateButton->setObjectName("rotateByButton");
    connect(rotateButton, &QPushButton::clicked, this, &TransformPanel::onRotateByClicked);
    scaleBySpin_ = new QDoubleSpinBox(tab);
    scaleBySpin_->setObjectName("scaleBySpin");
    scaleBySpin_->setRange(-99.0, 1000.0);
    scaleBySpin_->setValue(10.0);
    scaleBySpin_->setSuffix("%");
    auto* scaleButton = new QPushButton("Scale", tab);
    scaleButton->setObjectName("scaleByButton");
    connect(scaleButton, &QPushButton::clicked, this, &TransformPanel::onScaleByClicked);
    rotateScaleRow->addWidget(rotateBySpin_);
    rotateScaleRow->addWidget(rotateButton);
    rotateScaleRow->addWidget(scaleBySpin_);
    rotateScaleRow->addWidget(scaleButton);
    layout->addLayout(rotateScaleRow);

    auto* exactToolsRow = new QHBoxLayout();
    auto* rotate90CWButton = new QPushButton("90\xC2\xB0 CW", tab);
    rotate90CWButton->setObjectName("rotate90CWButton");
    connect(rotate90CWButton, &QPushButton::clicked, this, &TransformPanel::onRotate90CWClicked);
    auto* rotate90CCWButton = new QPushButton("90\xC2\xB0 CCW", tab);
    rotate90CCWButton->setObjectName("rotate90CCWButton");
    connect(rotate90CCWButton, &QPushButton::clicked, this, &TransformPanel::onRotate90CCWClicked);
    auto* flipHButton = new QPushButton("Flip H", tab);
    flipHButton->setObjectName("flipHorizontalButton");
    connect(flipHButton, &QPushButton::clicked, this, &TransformPanel::onFlipHorizontalClicked);
    auto* flipVButton = new QPushButton("Flip V", tab);
    flipVButton->setObjectName("flipVerticalButton");
    connect(flipVButton, &QPushButton::clicked, this, &TransformPanel::onFlipVerticalClicked);
    exactToolsRow->addWidget(rotate90CWButton);
    exactToolsRow->addWidget(rotate90CCWButton);
    exactToolsRow->addWidget(flipHButton);
    exactToolsRow->addWidget(flipVButton);
    layout->addLayout(exactToolsRow);

    layout->addStretch(1);
    return tab;
}

QWidget* TransformPanel::buildColorsTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    colorSpin_ = new SliderSpin("Color", 0.0, 1.0, 0.0, 3, tab);
    colorSpin_->setObjectName("colorSpin");
    connect(colorSpin_, &SliderSpin::valueChanged, this, &TransformPanel::onColorChanged);
    connect(colorSpin_, &SliderSpin::editingFinished, this, &TransformPanel::onColorEditingFinished);
    layout->addWidget(colorSpin_);

    auto* swatchRow = new QHBoxLayout();
    swatchRow->addWidget(new QLabel("Preview", tab));
    colorSwatch_ = new QLabel(tab);
    colorSwatch_->setObjectName("colorSwatch");
    colorSwatch_->setFixedSize(48, 20);
    colorSwatch_->setAutoFillBackground(true);
    swatchRow->addWidget(colorSwatch_);
    swatchRow->addStretch(1);
    layout->addLayout(swatchRow);

    colorSpeedSpin_ = new SliderSpin("Color Speed", -1.0, 1.0, 0.0, 3, tab);
    colorSpeedSpin_->setObjectName("colorSpeedSpin");
    connect(colorSpeedSpin_, &SliderSpin::valueChanged, this, &TransformPanel::onColorSpeedChanged);
    connect(colorSpeedSpin_, &SliderSpin::editingFinished, this, &TransformPanel::onColorSpeedEditingFinished);
    layout->addWidget(colorSpeedSpin_);

    opacitySpin_ = new SliderSpin("Opacity", 0.0, 1.0, 1.0, 3, tab);
    opacitySpin_->setObjectName("opacitySpin");
    connect(opacitySpin_, &SliderSpin::valueChanged, this, &TransformPanel::onOpacityChanged);
    connect(opacitySpin_, &SliderSpin::editingFinished, this, &TransformPanel::onOpacityEditingFinished);
    layout->addWidget(opacitySpin_);

    directColorSpin_ = new SliderSpin("Direct Color", 0.0, 1.0, 1.0, 3, tab);
    directColorSpin_->setObjectName("directColorSpin");
    connect(directColorSpin_, &SliderSpin::valueChanged, this, &TransformPanel::onDirectColorChanged);
    connect(directColorSpin_, &SliderSpin::editingFinished, this, &TransformPanel::onDirectColorEditingFinished);
    layout->addWidget(directColorSpin_);

    soloCheck_ = new QCheckBox("Solo this transform", tab);
    soloCheck_->setObjectName("soloCheck");
    soloCheck_->setToolTip("Mute every other transform when rendering, to see this one in isolation.");
    connect(soloCheck_, &QCheckBox::toggled, this, &TransformPanel::onSoloToggled);
    layout->addWidget(soloCheck_);

    layout->addStretch(1);
    return tab;
}

QWidget* TransformPanel::buildVariablesTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    variablesTable_ = new QTableWidget(tab);
    variablesTable_->setObjectName("variablesTable");
    variablesTable_->setColumnCount(2);
    variablesTable_->setHorizontalHeaderLabels({"Name", "Value"});
    variablesTable_->horizontalHeader()->setStretchLastSection(true);
    variablesTable_->verticalHeader()->setVisible(false);

    const auto& registry = apo::VariationRegistry::instance();
    const int n = registry.numVariableNames();
    variablesTable_->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(registry.variableNameAt(i)));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        variablesTable_->setItem(i, 0, nameItem);
        variablesTable_->setItem(i, 1, new QTableWidgetItem());
    }
    connect(variablesTable_, &QTableWidget::itemChanged, this, &TransformPanel::onVariableCellChanged);
    layout->addWidget(variablesTable_, /*stretch=*/1);

    return tab;
}

apo::XForm* TransformPanel::currentXForm() const {
    if (!flame_) return nullptr;
    if (selectedIndex_ == apo::kFinalXformIndex) {
        return flame_->finalXformEnabled ? flame_->finalXform.get() : nullptr;
    }
    if (selectedIndex_ < 0 || selectedIndex_ >= flame_->numXForms()) return nullptr;
    return flame_->xform[static_cast<size_t>(selectedIndex_)].get();
}

void TransformPanel::setFlame(std::shared_ptr<apo::Flame> flame) {
    flame_ = std::move(flame);
    setSelectedXform(selectedIndex_);
}

void TransformPanel::setSelectedXform(int index) {
    selectedIndex_ = index;
    setEnabled(currentXForm() != nullptr);
    refreshHeaderFields();
    refreshVariationsValues();
    refreshColorsTab();
    refreshVariablesValues();
    refreshTransformTab();
}

void TransformPanel::refreshHeaderFields() {
    const apo::XForm* xf = currentXForm();

    nameEdit_->blockSignals(true);
    nameEdit_->setText(xf ? QString::fromStdString(xf->transformName) : QString());
    nameEdit_->blockSignals(false);

    weightSpin_->blockSignals(true);
    weightSpin_->setValue(xf ? std::max(kMinWeight, xf->density) : kMinWeight);
    weightSpin_->blockSignals(false);
}

void TransformPanel::refreshVariationsValues() {
    if (!variationsTable_) return;
    const apo::XForm* xf = currentXForm();
    const auto& registry = apo::VariationRegistry::instance();

    variationsTable_->blockSignals(true);
    for (int i = 0; i < registry.nrVar(); ++i) {
        variationsTable_->item(i, 1)->setText(formatValue(xf ? xf->variation(i) : 0.0));
    }
    variationsTable_->blockSignals(false);
    applyVariationsRowFilter();
}

void TransformPanel::applyVariationsRowFilter() {
    if (!variationsTable_) return;
    const apo::XForm* xf = currentXForm();
    const QString search = variationSearchEdit_->text();
    const bool hideUnused = hideUnusedCheck_->isChecked();

    for (int i = 0; i < variationsTable_->rowCount(); ++i) {
        const QString name = variationsTable_->item(i, 0)->text();
        bool visible = name.contains(search, Qt::CaseInsensitive);
        if (visible && hideUnused) {
            const double v = xf ? xf->variation(i) : 0.0;
            visible = v != 0.0;
        }
        variationsTable_->setRowHidden(i, !visible);
    }
}

void TransformPanel::onVariationSearchChanged(const QString&) { applyVariationsRowFilter(); }
void TransformPanel::onHideUnusedToggled(bool) { applyVariationsRowFilter(); }

void TransformPanel::onShowDescriptionsToggled(bool show) {
    if (!variationsTable_) return;
    variationsTable_->setColumnHidden(2, !show);
}

void TransformPanel::onClearVariationsClicked() {
    apo::XForm* xf = currentXForm();
    if (!xf) return;

    bool anyNonZero = false;
    for (int i = 0; i < xf->numVariations(); ++i) {
        if (xf->variation(i) != 0.0) {
            anyNonZero = true;
            break;
        }
    }
    if (!anyNonZero) return; // nothing to do - don't clutter undo with a no-op

    beginEditIfNeeded();
    for (int i = 0; i < xf->numVariations(); ++i) xf->setVariation(i, 0.0);
    refreshVariationsValues();
    refreshVariablesValues(); // clearing weights can hide every Variables-tab row
    emit propertyEdited();
    commitEditIfNeeded();
}

void TransformPanel::onVariationCellChanged(QTableWidgetItem* item) {
    if (item->column() != 1) return;
    apo::XForm* xf = currentXForm();
    if (!xf) return;

    const int varIndex = item->row();
    bool ok = false;
    const double newValue = item->text().toDouble(&ok);
    const double oldValue = xf->variation(varIndex);

    variationsTable_->blockSignals(true);
    if (!ok) {
        item->setText(formatValue(oldValue)); // invalid input: revert display, no-op edit
    } else if (newValue != oldValue) {
        beginEditIfNeeded();
        xf->setVariation(varIndex, newValue);
        item->setText(formatValue(newValue));
        variationsTable_->blockSignals(false);
        if (hideUnusedCheck_->isChecked()) applyVariationsRowFilter();
        refreshVariablesValues(); // this variation's own rows may have just appeared/disappeared
        emit propertyEdited();
        commitEditIfNeeded();
        return;
    }
    variationsTable_->blockSignals(false);
}

void TransformPanel::refreshColorsTab() {
    if (!colorSpin_) return;
    const apo::XForm* xf = currentXForm();

    colorSpin_->setValue(xf ? xf->color : 0.0);
    colorSpeedSpin_->setValue(xf ? xf->symmetry : 0.0);
    opacitySpin_->setValue(xf ? xf->transOpacity : 1.0);
    directColorSpin_->setValue(xf ? xf->pluginColor : 1.0);

    soloCheck_->blockSignals(true);
    soloCheck_->setChecked(xf && flame_->soloXform == selectedIndex_);
    soloCheck_->blockSignals(false);

    refreshColorSwatch();
}

void TransformPanel::refreshColorSwatch() {
    const apo::XForm* xf = currentXForm();
    int r = 0, g = 0, b = 0;
    if (flame_ && xf) {
        const double color = xf->color;
        const int idx = std::clamp(static_cast<int>(color * 255.0), 0, 255);
        const auto& entry = flame_->cmap.entries[static_cast<size_t>(idx)];
        r = entry[0];
        g = entry[1];
        b = entry[2];
    }
    QPalette pal = colorSwatch_->palette();
    pal.setColor(QPalette::Window, QColor(r, g, b));
    colorSwatch_->setPalette(pal);
}

void TransformPanel::refreshVariablesValues() {
    if (!variablesTable_) return;
    const apo::XForm* xf = currentXForm();
    const auto& registry = apo::VariationRegistry::instance();

    variablesTable_->blockSignals(true);
    for (int i = 0; i < registry.numVariableNames(); ++i) {
        const int varIndex = registry.variationIndexFromVariableNameIndex(i);
        const bool inUse = xf && varIndex >= 0 && xf->variation(varIndex) != 0.0;
        variablesTable_->setRowHidden(i, !inUse);
        if (inUse) {
            double value = 0.0;
            xf->getVariable(registry.variableNameAt(i), value);
            variablesTable_->item(i, 1)->setText(formatValue(value));
        }
    }
    variablesTable_->blockSignals(false);
}

void TransformPanel::onVariableCellChanged(QTableWidgetItem* item) {
    if (item->column() != 1) return;
    apo::XForm* xf = currentXForm();
    if (!xf) return;

    const auto& registry = apo::VariationRegistry::instance();
    const std::string name = registry.variableNameAt(item->row());

    double oldValue = 0.0;
    xf->getVariable(name, oldValue);

    bool ok = false;
    double newValue = item->text().toDouble(&ok);

    variablesTable_->blockSignals(true);
    if (!ok) {
        item->setText(formatValue(oldValue)); // invalid input: revert display, no-op edit
    } else if (newValue != oldValue) {
        beginEditIfNeeded();
        xf->setVariable(name, newValue);
        item->setText(formatValue(newValue));
        variablesTable_->blockSignals(false);
        emit propertyEdited();
        commitEditIfNeeded();
        return;
    }
    variablesTable_->blockSignals(false);
}

void TransformPanel::onColorChanged(double value) {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    beginEditIfNeeded();
    xf->color = value;
    refreshColorSwatch();
    emit propertyEdited();
}

void TransformPanel::onColorEditingFinished(double) { commitEditIfNeeded(); }

void TransformPanel::onColorSpeedChanged(double value) {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    beginEditIfNeeded();
    xf->symmetry = value;
    emit propertyEdited();
}

void TransformPanel::onColorSpeedEditingFinished(double) { commitEditIfNeeded(); }

void TransformPanel::onOpacityChanged(double value) {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    beginEditIfNeeded();
    xf->transOpacity = value;
    emit propertyEdited();
}

void TransformPanel::onOpacityEditingFinished(double) { commitEditIfNeeded(); }

void TransformPanel::onDirectColorChanged(double value) {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    beginEditIfNeeded();
    xf->pluginColor = value;
    emit propertyEdited();
}

void TransformPanel::onDirectColorEditingFinished(double) { commitEditIfNeeded(); }

void TransformPanel::onSoloToggled(bool solo) {
    if (!flame_ || selectedIndex_ < 0) return;
    beginEditIfNeeded();
    flame_->soloXform = solo ? selectedIndex_ : -1;
    emit propertyEdited();
    commitEditIfNeeded(); // a checkbox click is already one atomic gesture
}

void TransformPanel::onWeightChanged(double value) {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    beginEditIfNeeded();
    xf->density = std::max(kMinWeight, value);
    emit propertyEdited();
}

void TransformPanel::onWeightEditingFinished() { commitEditIfNeeded(); }

void TransformPanel::onNameEdited(const QString& text) {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    beginEditIfNeeded();
    xf->transformName = text.toStdString();
    emit propertyEdited();
}

void TransformPanel::onNameEditingFinished() { commitEditIfNeeded(); }

void TransformPanel::beginEditIfNeeded() {
    if (hasPendingGesture_) return;
    hasPendingGesture_ = true;
    emit editingStarted();
}

void TransformPanel::commitEditIfNeeded() {
    if (!hasPendingGesture_) return;
    hasPendingGesture_ = false;
    emit editingFinished();
}

std::array<std::array<double, 2>, 3>& TransformPanel::activeMatrix(apo::XForm& xf) const {
    return editingPost_ ? xf.p : xf.c;
}

void TransformPanel::setEditingPostTransform(bool post) {
    if (editingPost_ == post) return;
    editingPost_ = post;
    if (editPostCheck_) {
        editPostCheck_->blockSignals(true);
        editPostCheck_->setChecked(post);
        editPostCheck_->blockSignals(false);
    }
    refreshTransformTab();
}

void TransformPanel::refreshTransformTab() {
    if (!vertexOX_) return;
    apo::XForm* xf = currentXForm();
    const apo::Triangle t = xf ? apo::triangleFromCoefs(activeMatrix(*xf)) : apo::Triangle{};

    auto setSpin = [](QDoubleSpinBox* box, double v) {
        box->blockSignals(true);
        box->setValue(v);
        box->blockSignals(false);
    };
    setSpin(vertexOX_, t.o.x);
    setSpin(vertexOY_, t.o.y);
    setSpin(vertexXX_, t.x.x);
    setSpin(vertexXY_, t.x.y);
    setSpin(vertexYX_, t.y.x);
    setSpin(vertexYY_, t.y.y);
}

void TransformPanel::applyVertexEdit() {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    apo::Triangle t;
    t.o = {vertexOX_->value(), vertexOY_->value()};
    t.x = {vertexXX_->value(), vertexXY_->value()};
    t.y = {vertexYX_->value(), vertexYY_->value()};
    activeMatrix(*xf) = apo::coefsFromTriangle(t);
}

void TransformPanel::transformActiveTriangle(const std::function<apo::Point2(apo::Point2, apo::Point2)>& op) {
    apo::XForm* xf = currentXForm();
    if (!xf) return;
    auto& mat = activeMatrix(*xf);
    apo::Triangle t = apo::triangleFromCoefs(mat);
    const apo::Point2 pivot = apo::centroid(t);

    beginEditIfNeeded();
    t.o = op(pivot, t.o);
    t.x = op(pivot, t.x);
    t.y = op(pivot, t.y);
    mat = apo::coefsFromTriangle(t);
    refreshTransformTab();
    emit propertyEdited();
    commitEditIfNeeded();
}

void TransformPanel::onEditPostToggled(bool post) {
    editingPost_ = post;
    refreshTransformTab();
    emit editingPostTransformChanged(post);
}

void TransformPanel::onVertexFieldChanged() {
    if (!currentXForm()) return;
    beginEditIfNeeded();
    applyVertexEdit();
    emit propertyEdited();
}

void TransformPanel::onVertexFieldEditingFinished() { commitEditIfNeeded(); }

void TransformPanel::onRotateByClicked() {
    const double radians = rotateBySpin_->value() * kPi / 180.0;
    transformActiveTriangle(
        [radians](apo::Point2 pivot, apo::Point2 p) { return apo::rotateAroundPivot(p, pivot, radians); });
}

void TransformPanel::onScaleByClicked() {
    const double factor = 1.0 + scaleBySpin_->value() / 100.0;
    transformActiveTriangle(
        [factor](apo::Point2 pivot, apo::Point2 p) { return apo::scaleAroundPivot(p, pivot, factor); });
}

void TransformPanel::onRotate90CWClicked() {
    transformActiveTriangle(
        [](apo::Point2 pivot, apo::Point2 p) { return apo::rotateAroundPivot(p, pivot, -kPi / 2.0); });
}

void TransformPanel::onRotate90CCWClicked() {
    transformActiveTriangle(
        [](apo::Point2 pivot, apo::Point2 p) { return apo::rotateAroundPivot(p, pivot, kPi / 2.0); });
}

void TransformPanel::onFlipHorizontalClicked() {
    transformActiveTriangle([](apo::Point2 pivot, apo::Point2 p) { return apo::Point2{2 * pivot.x - p.x, p.y}; });
}

void TransformPanel::onFlipVerticalClicked() {
    transformActiveTriangle([](apo::Point2 pivot, apo::Point2 p) { return apo::Point2{p.x, 2 * pivot.y - p.y}; });
}

} // namespace apo::ui
