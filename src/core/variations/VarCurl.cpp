#include "VarCurl.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
// Approximates Delphi's Math.IsZero default-epsilon check, used by the
// original's GetCalcFunction to pick a specialized branch.
bool isZero(double x) { return std::fabs(x) < 1e-12; }
} // namespace

void VarCurl::prepare() { c2x2_ = 2 * c2_; }

void VarCurl::calc() {
    const double re = 1 + c1_ * (*tx) + c2_ * ((*tx) * (*tx) - (*ty) * (*ty));
    const double im = c1_ * (*ty) + c2x2_ * (*tx) * (*ty);

    const double r = vvar / (re * re + im * im);

    *px += (*tx * re + *ty * im) * r;
    *py += (*ty * re - *tx * im) * r;
    *pz += vvar * *tz;
}

void VarCurl::calcZeroC2() {
    const double re = 1 + c1_ * (*tx);
    const double im = c1_ * (*ty);

    const double r = vvar / (re * re + im * im);

    *px += (*tx * re + *ty * im) * r;
    *py += (*ty * re - *tx * im) * r;
    *pz += vvar * *tz;
}

void VarCurl::calcZeroC1() {
    const double re = 1 + c2_ * ((*tx) * (*tx) - (*ty) * (*ty));
    const double im = c2x2_ * (*tx) * (*ty);

    const double r = vvar / (re * re + im * im);

    *px += (*tx * re + *ty * im) * r;
    *py += (*ty * re - *tx * im) * r;
    *pz += vvar * *tz;
}

void VarCurl::calcZeroC2C1() {
    *px += vvar * *tx;
    *py += vvar * *ty;
    *pz += vvar * *tz;
}

CalcFn VarCurl::selectCalcFunction() {
    if (isZero(c1_)) {
        if (isZero(c2_)) return bindCalc<VarCurl, &VarCurl::calcZeroC2C1>(this);
        return bindCalc<VarCurl, &VarCurl::calcZeroC1>(this);
    }
    if (isZero(c2_)) return bindCalc<VarCurl, &VarCurl::calcZeroC2>(this);
    return bindCalc<VarCurl, &VarCurl::calc>(this);
}

std::string VarCurl::variableNameAt(int index) const {
    switch (index) {
        case 0: return "curl_c1";
        case 1: return "curl_c2";
        default: return "";
    }
}

bool VarCurl::getVariable(const std::string& name, double& value) const {
    if (name == "curl_c1") { value = c1_; return true; }
    if (name == "curl_c2") { value = c2_; return true; }
    return false;
}

bool VarCurl::setVariable(const std::string& name, double& value) {
    if (name == "curl_c1") { c1_ = value; return true; }
    if (name == "curl_c2") { c2_ = value; return true; }
    return false;
}

bool VarCurl::resetVariable(const std::string& name) {
    if (name == "curl_c1") { c1_ = 0; return true; }
    if (name == "curl_c2") { c2_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarCurl>();
}

} // namespace apo
