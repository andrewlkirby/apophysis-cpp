#include "XForm.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "VariationRegistry.h"

namespace apo {

namespace {
constexpr double kEps = 1e-300; // matches XForm.pas's EPS
constexpr double kPi = 3.14159265358979323846;

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
} // namespace

XForm::XForm() {
    addRegVariations();
    buildFunctionList();
    vars_.assign(kNumLocalVars + regVariations_.size(), 0.0);
    clear();
}

XForm::~XForm() = default;

void XForm::clear() {
    density = 0;
    color = 0;
    symmetry = 0;
    postXswap = false;
    autoZscale = false;

    c = {{{1, 0}, {0, 1}, {0, 0}}};
    p = {{{1, 0}, {0, 1}, {0, 0}}};

    std::fill(vars_.begin(), vars_.end(), 0.0);
    if (!vars_.empty()) vars_[0] = 1;

    modWeights.fill(1.0);

    transOpacity = 1;
    pluginColor = 1;
}

void XForm::addRegVariations() {
    // No Variation instances created here - just sizes the slot array (see
    // regVariations_'s own doc comment in XForm.h). Every entry starts
    // null; ensureRegVariation() constructs one lazily the first time
    // variation i actually needs a live object.
    auto& registry = VariationRegistry::instance();
    regVariations_.resize(registry.numRegisteredVariations()); // default-constructs each slot to nullptr
}

Variation& XForm::ensureRegVariation(int i) const {
    if (!regVariations_[i]) {
        auto& registry = VariationRegistry::instance();
        regVariations_[i] = registry.registeredVariation(i).create();
        functionList_[kNumLocalVars + i] = bindCalc<Variation, &Variation::calc>(regVariations_[i].get());
    }
    return *regVariations_[i];
}

void XForm::buildFunctionList() {
    // Registered-variation slots (kNumLocalVars..) are left at their
    // default CalcFn{} (null obj/trampoline) here - ensureRegVariation()
    // binds each one the first time it's actually constructed. Safe: the
    // only place a registered slot is ever pushed into calcFunctionList_
    // (prepare(), below) always calls ensureRegVariation() for that same
    // index first, in the same call, so a still-default entry can never
    // actually be invoked.
    functionList_.assign(kNumLocalVars + regVariations_.size(), {});

    functionList_[0] = bindCalc<XForm, &XForm::linear3D>(this);
    functionList_[1] = bindCalc<XForm, &XForm::flatten>(this);
    functionList_[2] = bindCalc<XForm, &XForm::sinusoidal>(this);
    functionList_[3] = bindCalc<XForm, &XForm::spherical>(this);
    functionList_[4] = bindCalc<XForm, &XForm::swirl>(this);
    functionList_[5] = bindCalc<XForm, &XForm::horseshoe>(this);
    functionList_[6] = bindCalc<XForm, &XForm::polar>(this);
    functionList_[7] = bindCalc<XForm, &XForm::disc>(this);
    functionList_[8] = bindCalc<XForm, &XForm::spiral>(this);
    functionList_[9] = bindCalc<XForm, &XForm::hyperbolic>(this);
    functionList_[10] = bindCalc<XForm, &XForm::square>(this);
    functionList_[11] = bindCalc<XForm, &XForm::eyefish>(this);
    functionList_[12] = bindCalc<XForm, &XForm::bubble>(this);
    functionList_[13] = bindCalc<XForm, &XForm::cylinder>(this);
    functionList_[14] = bindCalc<XForm, &XForm::noise>(this);
    functionList_[15] = bindCalc<XForm, &XForm::blur>(this);
    functionList_[16] = bindCalc<XForm, &XForm::gaussian>(this);
    functionList_[17] = bindCalc<XForm, &XForm::zBlur>(this);
    functionList_[18] = bindCalc<XForm, &XForm::blur3D>(this);
    functionList_[19] = bindCalc<XForm, &XForm::preBlur>(this);
    functionList_[20] = bindCalc<XForm, &XForm::preZScale>(this);
    functionList_[21] = bindCalc<XForm, &XForm::preZTranslate>(this);
    functionList_[22] = bindCalc<XForm, &XForm::preRotateX>(this);
    functionList_[23] = bindCalc<XForm, &XForm::preRotateY>(this);
    functionList_[24] = bindCalc<XForm, &XForm::zScale>(this);
    functionList_[25] = bindCalc<XForm, &XForm::zTranslate>(this);
    functionList_[26] = bindCalc<XForm, &XForm::zCone>(this);
    functionList_[27] = bindCalc<XForm, &XForm::postRotateX>(this);
    functionList_[28] = bindCalc<XForm, &XForm::postRotateY>(this);
}

void XForm::prepare(Rng& rng) {
    rng_ = &rng;

    opacityAlwaysPasses_ = (transOpacity == 1.0);

    c00_ = c[0][0];
    c01_ = c[0][1];
    c10_ = c[1][0];
    c11_ = c[1][1];
    c20_ = c[2][0];
    c21_ = c[2][1];

    colorC1_ = (1 + symmetry) / 2.0;
    colorC2_ = color * (1 - symmetry) / 2.0;

    auto& registry = VariationRegistry::instance();

    // Only variations with nonzero weight this frame get bound/prepared -
    // calc() is never invoked for a zero-weight variation regardless (the
    // dispatch-building loops below all gate on vars_[i] != 0.0 too), so
    // running prepare()/selectCalcFunction() for a dormant one would
    // ordinarily be dead work (same reasoning as opacityAlwaysPasses_, B3) -
    // EXCEPT for a variation flagged hasRngSideEffectInPrepare (currently
    // only radial_blur), whose prepare() draws from `rng` unconditionally;
    // skipping that for a zero-weight instance would silently consume fewer
    // Rng draws than before for every flame, not just ones using it,
    // desyncing the deterministic point stream for everything rendered
    // afterward (see VariationFactory's own doc comment). This is also the
    // single materialization choke-point for weight-driven variations
    // (FOLLOWUP_PLAN.txt B1(b)) - ensureRegVariation() constructs the
    // instance the first time it's actually needed.
    for (size_t i = 0; i < regVariations_.size(); ++i) {
        const bool weighted = vars_[kNumLocalVars + i] != 0.0;
        if (!weighted && !registry.registeredVariation(static_cast<int>(i)).hasRngSideEffectInPrepare) continue;
        auto& v = ensureRegVariation(static_cast<int>(i));
        v.tx = &tx_;
        v.ty = &ty_;
        v.tz = &tz_;
        v.px = &px_;
        v.py = &py_;
        v.pz = &pz_;
        v.color = &vc_;
        v.a = c00_;
        v.b = c01_;
        v.c = c10_;
        v.d = c11_;
        v.e = c20_;
        v.f = c21_;
        v.vvar = vars_[kNumLocalVars + i];
        v.rng = &rng;
        v.prepare();
        // Matches TXForm.Prepare calling GetCalcFunction right after
        // Prepare on each registered variation, every single prepare() -
        // not just once at construction - since a few variations (e.g.
        // julian) pick a different specialized calc path depending on
        // their current parameter values.
        functionList_[kNumLocalVars + i] = v.selectCalcFunction();
    }

    calcFunctionList_.clear();

    const int nrVar = kNumLocalVars + static_cast<int>(regVariations_.size());

    const bool calculateAngle = (vars_[6] != 0.0) || (vars_[7] != 0.0);
    const bool calculateSinCos = (vars_[8] != 0.0) || (vars_[10] != 0.0);

    // Pre-variations first.
    for (int i = 0; i < nrVar; ++i) {
        if (vars_[i] != 0.0 && startsWith(registry.varName(i), "pre_")) {
            calcFunctionList_.push_back(functionList_[i]);
        }
    }

    // Precalc must run after pre-variations (they can perturb tx_/ty_/tz_).
    if (calculateAngle || calculateSinCos) {
        if (calculateAngle && calculateSinCos) {
            calcFunctionList_.push_back(bindCalc<XForm, &XForm::precalcAll>(this));
        } else if (calculateAngle) {
            calcFunctionList_.push_back(bindCalc<XForm, &XForm::precalcAngle>(this));
        } else {
            calcFunctionList_.push_back(bindCalc<XForm, &XForm::precalcSinCos>(this));
        }
    }

    // Normal variations.
    for (int i = 0; i < nrVar; ++i) {
        if (vars_[i] == 0.0) continue;
        const std::string name = registry.varName(i);
        if (startsWith(name, "pre_") || startsWith(name, "post_") || name == "flatten") continue;
        calcFunctionList_.push_back(functionList_[i]);
    }

    // Post-variations (and flatten, which behaves as a post step despite
    // its name not carrying the post_ prefix - matches XForm.pas exactly).
    for (int i = 0; i < nrVar; ++i) {
        if (vars_[i] == 0.0) continue;
        const std::string name = registry.varName(i);
        if (startsWith(name, "post_") || name == "flatten") {
            calcFunctionList_.push_back(functionList_[i]);
        }
    }

    polarVpi_ = vars_[6] / kPi;
    discVpi_ = vars_[7] / kPi;

    gaussRnd_[0] = rng_->uniform01();
    gaussRnd_[1] = rng_->uniform01();
    gaussRnd_[2] = rng_->uniform01();
    gaussRnd_[3] = rng_->uniform01();
    gaussN_ = 0;

    rxSin_ = std::sin(vars_[22] * kPi / 2);
    rxCos_ = std::cos(vars_[22] * kPi / 2);
    rySin_ = std::sin(vars_[23] * kPi / 2);
    ryCos_ = std::cos(vars_[23] * kPi / 2);

    pxSin_ = std::sin(vars_[27] * kPi / 2);
    pxCos_ = std::cos(vars_[27] * kPi / 2);
    pySin_ = std::sin(vars_[28] * kPi / 2);
    pyCos_ = std::cos(vars_[28] * kPi / 2);

    hasPostTransform_ = (p[0][0] != 1) || (p[0][1] != 0) || (p[1][0] != 0) || (p[1][1] != 1) ||
                         (p[2][0] != 0) || (p[2][1] != 0);
    if (hasPostTransform_) {
        p00_ = p[0][0];
        p01_ = p[0][1];
        p10_ = p[1][0];
        p11_ = p[1][1];
        p20_ = p[2][0];
        p21_ = p[2][1];
        calcFunctionList_.push_back(bindCalc<XForm, &XForm::doPostTransform>(this));
    }
}

void XForm::nextPoint(Point3& pt, double& colorCoord) {
    colorCoord = colorCoord * colorC1_ + colorC2_;
    vc_ = colorCoord;

    tx_ = c00_ * pt.x + c10_ * pt.y + c20_;
    ty_ = c01_ * pt.x + c11_ * pt.y + c21_;
    tz_ = pt.z;

    px_ = 0;
    py_ = 0;
    pz_ = 0;

    for (auto& fn : calcFunctionList_) fn();

    colorCoord = colorCoord + pluginColor * (vc_ - colorCoord);
    pt.x = px_;
    pt.y = py_;
    pt.z = pz_;
}

bool XForm::getVariable(const std::string& name, double& value) const {
    auto& registry = VariationRegistry::instance();
    const int idx = registry.variationIndexForVariableName(name);
    if (idx < kNumLocalVars) return false; // unknown name, or a local variation (no named parameters)
    const int i = idx - kNumLocalVars;
    // Always materialize via ensureRegVariation() (not a throwaway
    // factory.create() read): a variation flagged
    // hasNonDeterministicConstructionDefault draws a fresh random value
    // every time it's constructed, so a "peek without allocating" read
    // would return a *different* answer on every call - freezing the first
    // real read via ensureRegVariation() instead makes repeated reads of
    // the same untouched XForm stable, matching every other variation's
    // already-idempotent behavior.
    return ensureRegVariation(i).getVariable(name, value);
}

bool XForm::setVariable(const std::string& name, double& value) {
    auto& registry = VariationRegistry::instance();
    const int idx = registry.variationIndexForVariableName(name);
    if (idx < kNumLocalVars) return false;
    return ensureRegVariation(idx - kNumLocalVars).setVariable(name, value);
}

bool XForm::resetVariable(const std::string& name) {
    // Delegates to the owning Variation's own resetVariable() override -
    // NOT setVariable(name, 0), since many variations override
    // resetVariable with a non-zero default (e.g. auger_freq resets to 5).
    // Matches TXForm.ResetVariable.
    //
    // Always materializes via ensureRegVariation(), rather than treating an
    // unmaterialized slot as "already reset": a reset value is not
    // guaranteed to equal a fresh instance's own construction default (true
    // for every hasNonDeterministicConstructionDefault variation, whose
    // construction default is random while its reset value is fixed; also
    // already true for auger's own resetVariable, which - matching a
    // faithfully-ported typo in the original Pascal source, see
    // VarAuger.cpp - resets "auger_scale" to auger_sym_'s default instead
    // of its own). Constructing here to find that out is the only correct
    // option; short-circuiting "unmaterialized -> true" would have silently
    // skipped a real reset for any such case.
    auto& registry = VariationRegistry::instance();
    const int idx = registry.variationIndexForVariableName(name);
    if (idx < kNumLocalVars) return false;
    return ensureRegVariation(idx - kNumLocalVars).resetVariable(name);
}

void XForm::assign(const XForm& other) {
    vars_ = other.vars_;
    c = other.c;
    p = other.p;
    density = other.density;
    color = other.color;
    color2 = other.color2;
    symmetry = other.symmetry;
    orientationType = other.orientationType;
    transformName = other.transformName;
    postXswap = other.postXswap;
    autoZscale = other.autoZscale;

    const size_t n = std::min(regVariations_.size(), other.regVariations_.size());
    auto& registry = VariationRegistry::instance();
    for (size_t i = 0; i < n; ++i) {
        const auto& factory = registry.registeredVariation(static_cast<int>(i));
        if (!other.regVariations_[i]) {
            // hasNonDeterministicConstructionDefault (Variation.h): other's
            // unmaterialized slot does NOT stand for one fixed, reproducible
            // value for this type - it's whatever a fresh construction
            // would randomly draw - so the null-skip below isn't safe here.
            // Force other to actually freeze a real value first (via its
            // own ensureRegVariation(), not a throwaway - this is exactly
            // the same "first real read must permanently materialize"
            // requirement getVariable() has), then fall through to copy it
            // like any materialized source.
            if (factory.hasNonDeterministicConstructionDefault) {
                other.ensureRegVariation(static_cast<int>(i));
            } else {
                // other is at this variation type's factory default (never
                // touched it) - this must end up matching, so drop any
                // materialized (possibly customized) copy of our own rather
                // than leaving it stale. Null already means exactly "at
                // default" (see regVariations_'s own doc comment), so
                // there's nothing further to copy.
                regVariations_[i].reset();
                continue;
            }
        }
        Variation& dst = ensureRegVariation(static_cast<int>(i));
        const int nVars = factory.numVariables();
        for (int j = 0; j < nVars; ++j) {
            const std::string name = factory.variableNameAt(j);
            double value = 0;
            other.regVariations_[i]->getVariable(name, value);
            dst.setVariable(name, value);
        }
    }

    modWeights = other.modWeights;
    transOpacity = other.transOpacity;
    pluginColor = other.pluginColor;
}

void XForm::interpolateVariablesFrom(const XForm& x1, const XForm& x2, double c0, double c1) {
    // Same FRegVariations-alignment assumption XForm::assign() relies on:
    // every XForm's regVariations_ covers the same registered variation
    // types in the same order (populated identically by addRegVariations()),
    // so walking (i, j) directly - rather than searching by name - is safe.
    auto& registry = VariationRegistry::instance();
    for (size_t i = 0; i < regVariations_.size(); ++i) {
        const auto& factory = registry.registeredVariation(static_cast<int>(i));
        const int nVars = factory.numVariables();
        if (nVars == 0) continue;

        // hasNonDeterministicConstructionDefault (Variation.h): an
        // unmaterialized side does NOT stand for one fixed, reproducible
        // value for this type, so neither the c0+c1==1 skip below nor a
        // throwaway-instance read (which would draw a fresh, different
        // random value every call) is safe - force both sides to actually
        // freeze a real value first via their own ensureRegVariation()
        // (the same "first real read must permanently materialize"
        // requirement getVariable() has), then fall through to the normal
        // blend using those now-real instances.
        if (factory.hasNonDeterministicConstructionDefault) {
            x1.ensureRegVariation(static_cast<int>(i));
            x2.ensureRegVariation(static_cast<int>(i));
        } else if (!x1.regVariations_[i] && !x2.regVariations_[i]) {
            // Neither keyframe ever customized this one - blending two
            // copies of the same factory default. When c0+c1==1 (true of
            // MutationOps::blendXforms's only real caller, c0=1-t/c1=t)
            // that reproduces the same default exactly, so skip entirely
            // rather than materializing our own copy just to write back
            // what's already there.
            if (c0 + c1 == 1.0) {
                regVariations_[i].reset();
                continue;
            }
        }

        // A never-touched side (only possible here for a deterministic-
        // default type, since the non-deterministic branch above already
        // materialized both) reads as its type's real factory default via
        // a throwaway instance, without permanently materializing x1/x2.
        std::unique_ptr<Variation> tmp1, tmp2;
        const Variation* v1 = x1.regVariations_[i].get();
        if (!v1) {
            tmp1 = factory.create();
            v1 = tmp1.get();
        }
        const Variation* v2 = x2.regVariations_[i].get();
        if (!v2) {
            tmp2 = factory.create();
            v2 = tmp2.get();
        }

        Variation& dst = ensureRegVariation(static_cast<int>(i));
        for (int j = 0; j < nVars; ++j) {
            const std::string name = factory.variableNameAt(j);
            double val1 = 0, val2 = 0;
            v1->getVariable(name, val1);
            v2->getVariable(name, val2);
            double blended = c0 * val1 + c1 * val2;
            dst.setVariable(name, blended);
        }
    }
}

// ---- Local variations (ported from XForm.pas's non-asm branches) ---------

void XForm::linear3D() {
    px_ += vars_[0] * tx_;
    py_ += vars_[0] * ty_;
    pz_ += vars_[0] * tz_;
}

void XForm::flatten() { pz_ = 0; }

void XForm::sinusoidal() {
    px_ += vars_[2] * std::sin(tx_);
    py_ += vars_[2] * std::sin(ty_);
    pz_ += tz_ * vars_[2];
}

void XForm::spherical() {
    const double r = vars_[3] / (tx_ * tx_ + ty_ * ty_ + kEps);
    px_ += tx_ * r;
    py_ += ty_ * r;
    pz_ += tz_ * vars_[3];
}

void XForm::swirl() {
    const double t = tx_ * tx_ + ty_ * ty_;
    const double sinr = std::sin(t), cosr = std::cos(t);
    px_ += vars_[4] * (sinr * tx_ - cosr * ty_);
    py_ += vars_[4] * (cosr * tx_ + sinr * ty_);
    pz_ += tz_ * vars_[4];
}

void XForm::horseshoe() {
    const double r = vars_[5] / (std::sqrt(tx_ * tx_ + ty_ * ty_) + kEps);
    px_ += (tx_ - ty_) * (tx_ + ty_) * r;
    py_ += (2 * tx_ * ty_) * r;
    pz_ += tz_ * vars_[5];
}

void XForm::polar() {
    px_ += polarVpi_ * angle_;
    py_ += vars_[6] * (std::sqrt(tx_ * tx_ + ty_ * ty_) - 1.0);
    pz_ += tz_ * vars_[6];
}

void XForm::disc() {
    const double t = kPi * std::sqrt(tx_ * tx_ + ty_ * ty_);
    const double sinr = std::sin(t), cosr = std::cos(t);
    const double r = discVpi_ * angle_;
    px_ += sinr * r;
    py_ += cosr * r;
    pz_ += tz_ * vars_[7];
}

void XForm::spiral() {
    double r = length_ + 1e-6;
    const double sinr = std::sin(r), cosr = std::cos(r);
    r = vars_[8] / r;
    px_ += (cosA_ + sinr) * r;
    py_ += (sinA_ - cosr) * r;
    pz_ += tz_ * vars_[8];
}

void XForm::hyperbolic() {
    px_ += vars_[9] * tx_ / (tx_ * tx_ + ty_ * ty_ + kEps);
    py_ += vars_[9] * ty_;
    pz_ += tz_ * vars_[9];
}

void XForm::square() {
    const double sinr = std::sin(length_), cosr = std::cos(length_);
    px_ += vars_[10] * sinA_ * cosr;
    py_ += vars_[10] * cosA_ * sinr;
    pz_ += tz_ * vars_[10];
}

void XForm::eyefish() {
    const double r = 2 * vars_[11] / (std::sqrt(tx_ * tx_ + ty_ * ty_) + 1);
    px_ += r * tx_;
    py_ += r * ty_;
    pz_ += tz_ * vars_[11];
}

void XForm::bubble() {
    double r = (tx_ * tx_ + ty_ * ty_) / 4 + 1;
    pz_ += vars_[12] * (2 / r - 1);
    r = vars_[12] / r;
    px_ += r * tx_;
    py_ += r * ty_;
}

void XForm::cylinder() {
    px_ += vars_[13] * std::sin(tx_);
    py_ += vars_[13] * ty_;
    pz_ += vars_[13] * std::cos(tx_);
}

// noise/blur/gaussian_blur/zblur/blur3D/pre_blur are stochastic. The
// original called Delphi's RTL `Randomize` (reseed-from-clock) immediately
// before each `random` call inside these procedures - a "HACK! Fix me..."
// per the original's own comment, and unnecessary here since rng_ is
// already a full-period PRNG stream bound once per XForm::prepare(); this
// port intentionally drops the reseed rather than carrying the hack forward.

void XForm::noise() {
    const double ang = rng_->uniformAngle();
    const double sinr = std::sin(ang), cosr = std::cos(ang);
    const double s = vars_[14];
    const double r = s * rng_->uniform01();
    px_ += tx_ * r * cosr;
    py_ += ty_ * r * sinr;
    pz_ += tz_ * s;
}

void XForm::blur() {
    const double ang = rng_->uniformAngle();
    const double sina = std::sin(ang), cosa = std::cos(ang);
    const double s = vars_[15];
    const double z = tz_;
    const double r = s * rng_->uniform01();
    px_ += r * cosa;
    py_ += r * sina;
    pz_ += s * z;
}

void XForm::gaussian() {
    const double ang = rng_->uniformAngle();
    const double sina = std::sin(ang), cosa = std::cos(ang);
    const double s = vars_[16];
    const double z = tz_;
    const double r = s * (gaussRnd_[0] + gaussRnd_[1] + gaussRnd_[2] + gaussRnd_[3] - 2);
    gaussRnd_[gaussN_] = rng_->uniform01();
    gaussN_ = (gaussN_ + 1) & 3;

    px_ += r * cosa;
    py_ += r * sina;
    pz_ += s * z;
}

void XForm::zBlur() {
    pz_ += vars_[17] * (gaussRnd_[0] + gaussRnd_[1] + gaussRnd_[2] + gaussRnd_[3] - 2);
    gaussRnd_[gaussN_] = rng_->uniform01();
    gaussN_ = (gaussN_ + 1) & 3;
}

void XForm::blur3D() {
    const double ang = rng_->uniformAngle();
    const double sina = std::sin(ang), cosa = std::cos(ang);
    const double r = vars_[18] * (gaussRnd_[0] + gaussRnd_[1] + gaussRnd_[2] + gaussRnd_[3] - 2);
    gaussRnd_[gaussN_] = rng_->uniform01();
    gaussN_ = (gaussN_ + 1) & 3;

    const double angB = rng_->uniform01() * kPi;
    const double sinb = std::sin(angB), cosb = std::cos(angB);
    px_ += r * sinb * cosa;
    py_ += r * sinb * sina;
    pz_ += r * cosb;
}

void XForm::preBlur() {
    const double ang = rng_->uniformAngle();
    const double sina = std::sin(ang), cosa = std::cos(ang);
    const double r = vars_[19] * (gaussRnd_[0] + gaussRnd_[1] + gaussRnd_[2] + gaussRnd_[3] - 2);
    gaussRnd_[gaussN_] = rng_->uniform01();
    gaussN_ = (gaussN_ + 1) & 3;

    tx_ += r * cosa;
    ty_ += r * sina;
}

void XForm::preZScale() { tz_ *= vars_[20]; }

void XForm::preZTranslate() { tz_ += vars_[21]; }

void XForm::preRotateX() {
    const double z = rxCos_ * tz_ - rxSin_ * ty_;
    ty_ = rxSin_ * tz_ + rxCos_ * ty_;
    tz_ = z;
}

void XForm::preRotateY() {
    const double x = ryCos_ * tx_ - rySin_ * tz_;
    tz_ = rySin_ * tx_ + ryCos_ * tz_;
    tx_ = x;
}

void XForm::zScale() { pz_ += vars_[24] * tz_; }

void XForm::zTranslate() { pz_ += vars_[25]; }

void XForm::zCone() { pz_ += vars_[26] * std::sqrt(tx_ * tx_ + ty_ * ty_); }

void XForm::postRotateX() {
    const double z = pxCos_ * pz_ - pxSin_ * py_;
    py_ = pxSin_ * pz_ + pxCos_ * py_;
    pz_ = z;
}

void XForm::postRotateY() {
    const double x = pyCos_ * px_ - pySin_ * pz_;
    pz_ = pySin_ * px_ + pyCos_ * pz_;
    px_ = x;
}

void XForm::precalcAngle() { angle_ = std::atan2(tx_, ty_); }

void XForm::precalcSinCos() {
    length_ = std::sqrt(tx_ * tx_ + ty_ * ty_) + kEps;
    sinA_ = tx_ / length_;
    cosA_ = ty_ / length_;
}

void XForm::precalcAll() {
    length_ = std::sqrt(tx_ * tx_ + ty_ * ty_) + kEps;
    sinA_ = tx_ / length_;
    cosA_ = ty_ / length_;
    angle_ = std::atan2(tx_, ty_);
}

void XForm::doPostTransform() {
    const double tmp = px_;
    px_ = p00_ * px_ + p10_ * py_ + p20_;
    py_ = p01_ * tmp + p11_ * py_ + p21_;
}

// ---- Affine-coefficient helpers -------------------------------------------

XForm::Matrix3 XForm::identity() {
    Matrix3 m{};
    m[0][0] = 1;
    m[1][1] = 1;
    m[2][2] = 1;
    return m;
}

XForm::Matrix3 XForm::mul33(const Matrix3& m1, const Matrix3& m2) {
    Matrix3 result{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result[i][j] = m1[i][0] * m2[0][j] + m1[i][1] * m2[1][j] + m1[i][2] * m2[2][j];
        }
    }
    return result;
}

// Rotate/Translate/Multiply/Scale all follow the same read-modify-write
// shape on the affine coefs, packed into/out of a 3x3 matrix using row 2 as
// the translation column - matches TXForm's identical pattern in each of
// its four methods.

void XForm::rotate(double degrees) {
    const double r = degrees * kPi / 180.0;
    Matrix3 m1 = identity();
    m1[0][0] = std::cos(r);
    m1[0][1] = -std::sin(r);
    m1[1][0] = std::sin(r);
    m1[1][1] = std::cos(r);

    Matrix3 matrix = identity();
    matrix[0][0] = c[0][0];
    matrix[0][1] = c[0][1];
    matrix[1][0] = c[1][0];
    matrix[1][1] = c[1][1];
    matrix[0][2] = c[2][0];
    matrix[1][2] = c[2][1];

    matrix = mul33(matrix, m1);

    c[0][0] = matrix[0][0];
    c[0][1] = matrix[0][1];
    c[1][0] = matrix[1][0];
    c[1][1] = matrix[1][1];
    c[2][0] = matrix[0][2];
    c[2][1] = matrix[1][2];
}

void XForm::translate(double x, double y) {
    Matrix3 m1 = identity();
    m1[0][2] = x;
    m1[1][2] = y;

    Matrix3 matrix = identity();
    matrix[0][0] = c[0][0];
    matrix[0][1] = c[0][1];
    matrix[1][0] = c[1][0];
    matrix[1][1] = c[1][1];
    matrix[0][2] = c[2][0];
    matrix[1][2] = c[2][1];

    matrix = mul33(matrix, m1);

    c[0][0] = matrix[0][0];
    c[0][1] = matrix[0][1];
    c[1][0] = matrix[1][0];
    c[1][1] = matrix[1][1];
    c[2][0] = matrix[0][2];
    c[2][1] = matrix[1][2];
}

void XForm::multiply(double m00, double m01, double m10, double m11) {
    Matrix3 m1 = identity();
    m1[0][0] = m00;
    m1[0][1] = m01;
    m1[1][0] = m10;
    m1[1][1] = m11;

    Matrix3 matrix = identity();
    matrix[0][0] = c[0][0];
    matrix[0][1] = c[0][1];
    matrix[1][0] = c[1][0];
    matrix[1][1] = c[1][1];
    matrix[0][2] = c[2][0];
    matrix[1][2] = c[2][1];

    matrix = mul33(matrix, m1);

    c[0][0] = matrix[0][0];
    c[0][1] = matrix[0][1];
    c[1][0] = matrix[1][0];
    c[1][1] = matrix[1][1];
    c[2][0] = matrix[0][2];
    c[2][1] = matrix[1][2];
}

void XForm::scale(double s) {
    Matrix3 m1 = identity();
    m1[0][0] = s;
    m1[1][1] = s;

    Matrix3 matrix = identity();
    matrix[0][0] = c[0][0];
    matrix[0][1] = c[0][1];
    matrix[1][0] = c[1][0];
    matrix[1][1] = c[1][1];
    matrix[0][2] = c[2][0];
    matrix[1][2] = c[2][1];

    matrix = mul33(matrix, m1);

    c[0][0] = matrix[0][0];
    c[0][1] = matrix[0][1];
    c[1][0] = matrix[1][0];
    c[1][1] = matrix[1][1];
    c[2][0] = matrix[0][2];
    c[2][1] = matrix[1][2];
}

} // namespace apo
