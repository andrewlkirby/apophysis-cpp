#include "XForm.h"

#include <algorithm>
#include <cmath>

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
    auto& registry = VariationRegistry::instance();
    regVariations_.clear();
    regVariations_.reserve(registry.numRegisteredVariations());
    for (int i = 0; i < registry.numRegisteredVariations(); ++i) {
        regVariations_.push_back(registry.registeredVariation(i).create());
    }
}

void XForm::buildFunctionList() {
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

    for (size_t i = 0; i < regVariations_.size(); ++i) {
        functionList_[kNumLocalVars + i] = bindCalc<Variation, &Variation::calc>(regVariations_[i].get());
    }
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

    for (size_t i = 0; i < regVariations_.size(); ++i) {
        auto& v = *regVariations_[i];
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

    auto& registry = VariationRegistry::instance();
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
    for (auto& v : regVariations_) {
        if (v->getVariable(name, value)) return true;
    }
    return false;
}

bool XForm::setVariable(const std::string& name, double& value) {
    for (auto& v : regVariations_) {
        if (v->setVariable(name, value)) return true;
    }
    return false;
}

bool XForm::resetVariable(const std::string& name) {
    // Delegates to each Variation's own resetVariable() override, mirroring
    // getVariable/setVariable's loop shape above - NOT setVariable(name, 0),
    // since many variations override resetVariable with a non-zero default
    // (e.g. auger_freq resets to 5). Matches TXForm.ResetVariable.
    for (auto& v : regVariations_) {
        if (v->resetVariable(name)) return true;
    }
    return false;
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
    for (size_t i = 0; i < n; ++i) {
        const int nVars = regVariations_[i]->numVariables();
        for (int j = 0; j < nVars; ++j) {
            const std::string name = regVariations_[i]->variableNameAt(j);
            double value = 0;
            other.regVariations_[i]->getVariable(name, value);
            regVariations_[i]->setVariable(name, value);
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
    for (size_t i = 0; i < regVariations_.size(); ++i) {
        const int nVars = regVariations_[i]->numVariables();
        for (int j = 0; j < nVars; ++j) {
            const std::string name = regVariations_[i]->variableNameAt(j);
            double v1 = 0, v2 = 0;
            x1.regVariations_[i]->getVariable(name, v1);
            x2.regVariations_[i]->getVariable(name, v2);
            double blended = c0 * v1 + c1 * v2;
            regVariations_[i]->setVariable(name, blended);
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
