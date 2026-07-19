#include "VarRectangles.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kEps = 1e-10;
bool isZero(double x) { return std::fabs(x) < kEps; }
} // namespace

void VarRectangles::calc() {
    *px += vvar * ((2 * std::floor(*tx / rectanglesX_) + 1) * rectanglesX_ - *tx);
    *py += vvar * ((2 * std::floor(*ty / rectanglesY_) + 1) * rectanglesY_ - *ty);
    *pz += vvar * *tz;
}

void VarRectangles::calcZeroX() {
    *px += vvar * *tx;
    *py += vvar * ((2 * std::floor(*ty / rectanglesY_) + 1) * rectanglesY_ - *ty);
    *pz += vvar * *tz;
}

void VarRectangles::calcZeroY() {
    *px += vvar * ((2 * std::floor(*tx / rectanglesX_) + 1) * rectanglesX_ - *tx);
    *py += vvar * *ty;
    *pz += vvar * *tz;
}

void VarRectangles::calcZeroXY() {
    *px += vvar * *tx;
    *py += vvar * *ty;
    *pz += vvar * *tz;
}

CalcFn VarRectangles::selectCalcFunction() {
    if (isZero(rectanglesX_)) {
        if (isZero(rectanglesY_)) return bindCalc<VarRectangles, &VarRectangles::calcZeroXY>(this);
        return bindCalc<VarRectangles, &VarRectangles::calcZeroX>(this);
    }
    if (isZero(rectanglesY_)) return bindCalc<VarRectangles, &VarRectangles::calcZeroY>(this);
    return bindCalc<VarRectangles, &VarRectangles::calc>(this);
}

std::string VarRectangles::variableNameAt(int index) const {
    switch (index) {
        case 0: return "rectangles_x";
        case 1: return "rectangles_y";
        default: return "";
    }
}

bool VarRectangles::getVariable(const std::string& name, double& value) const {
    if (name == "rectangles_x") { value = rectanglesX_; return true; }
    if (name == "rectangles_y") { value = rectanglesY_; return true; }
    return false;
}

bool VarRectangles::setVariable(const std::string& name, double& value) {
    if (name == "rectangles_x") { rectanglesX_ = value; return true; }
    if (name == "rectangles_y") { rectanglesY_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarRectangles>();
}

} // namespace apo
