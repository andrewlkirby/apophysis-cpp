#include "Flame.h"

#include <algorithm>
#include <cstddef>

namespace apo {

Flame::Flame() {
    for (auto& x : xform) x = std::make_unique<XForm>();
    finalXform = std::make_unique<XForm>();
}

void Flame::clear() {
    for (auto& x : xform) x->clear();
    finalXform->clear();
    finalXformEnabled = false;
    useFinalXform = false;
    soloXform = -1;

    transparency = false;
    angle = 0;
    cameraPitch = cameraYaw = cameraPersp = cameraDOF = cameraZpos = 0;

    noLinearFix = false;
    cmap = ColorMap{};
    cmapIndex = 0;
    time = 0;
    brightness = 1.0;
    contrast = 1.0;
    gamma = 1.0;
    width = 100;
    height = 100;
    spatialOversample = 1;
    name.clear();
    nick.clear();
    url.clear();
    center = {0, 0};
    vibrancy = 1.0;
    hueRotation = 0.0;
    background = {0, 0, 0, 0};
    zoom = 0;
    pixelsPerUnit = 50;
    spatialFilterRadius = 0.5;
    sampleDensity = 50;
    actualDensity = 0;
    nbatches = 1;
    whiteLevel = 200;
    cmapInter = 0;
    symmetry = 0;
    pulse[0][0] = 0; pulse[0][1] = 60;
    pulse[1][0] = 0; pulse[1][1] = 60;
    wiggle[0][0] = 0; wiggle[0][1] = 60;
    wiggle[1][0] = 0; wiggle[1][1] = 60;

    estimator = 9.0;
    estimatorMin = 0.0;
    estimatorCurve = 0.4;
    jitters = 1;
    gammaThreshold = 0.01;
    enableDE = false;

    curves = std::array<BezierCurve, 4>{};

    xdata.clear();
}

std::unique_ptr<Flame> Flame::clone() const {
    auto result = std::make_unique<Flame>();
    result->copyFrom(*this);
    return result;
}

std::unique_ptr<Flame> Flame::cloneForRender(int activeXForms) const {
    auto result = std::unique_ptr<Flame>(new Flame(SkipXFormInitTag{}));
    activeXForms = std::clamp(activeXForms, 0, kMaxXForms);
    for (int i = 0; i < activeXForms; ++i) {
        result->xform[i] = std::make_unique<XForm>();
        result->xform[i]->assign(*xform[i]);
    }
    result->finalXform = std::make_unique<XForm>();
    result->finalXform->assign(*finalXform);
    result->copyScalarFieldsFrom(*this);
    return result;
}

void Flame::copyFrom(const Flame& other) {
    for (size_t i = 0; i < xform.size(); ++i) {
        xform[i]->assign(*other.xform[i]);
    }
    finalXform->assign(*other.finalXform);
    copyScalarFieldsFrom(other);
}

void Flame::copyScalarFieldsFrom(const Flame& other) {
    finalXformEnabled = other.finalXformEnabled;
    useFinalXform = other.useFinalXform;
    soloXform = other.soloXform;

    transparency = other.transparency;
    angle = other.angle;
    cameraPitch = other.cameraPitch;
    cameraYaw = other.cameraYaw;
    cameraPersp = other.cameraPersp;
    cameraDOF = other.cameraDOF;
    cameraZpos = other.cameraZpos;

    noLinearFix = other.noLinearFix;
    cmap = other.cmap;
    cmapIndex = other.cmapIndex;
    time = other.time;
    brightness = other.brightness;
    contrast = other.contrast;
    gamma = other.gamma;
    width = other.width;
    height = other.height;
    spatialOversample = other.spatialOversample;
    name = other.name;
    nick = other.nick;
    url = other.url;
    center = other.center;
    vibrancy = other.vibrancy;
    hueRotation = other.hueRotation;
    background = other.background;
    zoom = other.zoom;
    pixelsPerUnit = other.pixelsPerUnit;
    spatialFilterRadius = other.spatialFilterRadius;
    sampleDensity = other.sampleDensity;
    actualDensity = other.actualDensity;
    nbatches = other.nbatches;
    whiteLevel = other.whiteLevel;
    cmapInter = other.cmapInter;
    symmetry = other.symmetry;
    pulse[0][0] = other.pulse[0][0]; pulse[0][1] = other.pulse[0][1];
    pulse[1][0] = other.pulse[1][0]; pulse[1][1] = other.pulse[1][1];
    wiggle[0][0] = other.wiggle[0][0]; wiggle[0][1] = other.wiggle[0][1];
    wiggle[1][0] = other.wiggle[1][0]; wiggle[1][1] = other.wiggle[1][1];

    estimator = other.estimator;
    estimatorMin = other.estimatorMin;
    estimatorCurve = other.estimatorCurve;
    jitters = other.jitters;
    gammaThreshold = other.gammaThreshold;
    enableDE = other.enableDE;

    curves = other.curves;

    xdata = other.xdata;
}

int Flame::numXForms() const {
    for (int i = 0; i < kMaxXForms; ++i) {
        if (xform[i]->density == 0) return i;
    }
    return kMaxXForms;
}

int Flame::addXForm() {
    const int n = numXForms();
    if (n >= kMaxXForms) return -1;

    xform[n]->clear(); // density 0, linear (vars_[0]=1), modWeights all 1
    xform[n]->density = 0.5;
    return n;
}

int Flame::duplicateXForm(int index) {
    const int n = numXForms();
    if (n >= kMaxXForms || index < 0 || index >= n) return -1;

    xform[n]->assign(*xform[index]);
    for (int i = 0; i < n; ++i) {
        xform[i]->modWeights[n] = xform[i]->modWeights[index];
    }
    xform[n]->modWeights[n] = xform[index]->modWeights[index];
    return n;
}

void Flame::removeXForm(int index) {
    const int n = numXForms();
    if (index < 0 || index >= n || n <= 1) return;

    // Column-compact every remaining xform's xaos weights: drop the
    // deleted xform's column, shift every later column left by one to
    // match the row-compaction below, and reset the now-unused last
    // column back to the default weight.
    for (int i = 0; i < n; ++i) {
        for (int j = index; j < n - 1; ++j) {
            xform[i]->modWeights[j] = xform[i]->modWeights[j + 1];
        }
        xform[i]->modWeights[n - 1] = 1.0;
    }

    // Row-compact: shift every xform after `index` down by one slot.
    for (int i = index; i < n - 1; ++i) {
        xform[i]->assign(*xform[i + 1]);
    }
    xform[n - 1]->clear();

    if (soloXform > index) --soloXform;
    else if (soloXform == index) soloXform = -1;
}

void Flame::adjustScale(int w, int h) {
    // Matches ControlPoint.pas's AdjustScale: rescale pixels_per_unit
    // proportionally to the new width so the flame's on-screen composition
    // (how much of it is visible/how zoomed-in it looks) stays the same
    // when the render target's pixel dimensions change - e.g. the Adjust
    // dialog's Size tab, or the Editor canvas resizing.
    pixelsPerUnit = pixelsPerUnit * w / width;
    width = w;
    height = h;
}

} // namespace apo
