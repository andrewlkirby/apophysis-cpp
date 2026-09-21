#include "VarSupermoire.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kTwoPiOver3 = 2.0 * 3.14159265358979323846 / 3.0;
}

double VarSupermoire::triLattice(double x, double y, double angle, double freq) {
    double sum = 0;
    for (int k = 0; k < 3; ++k) {
        const double a = angle + k * kTwoPiOver3;
        sum += std::cos(freq * (x * std::cos(a) + y * std::sin(a)));
    }
    return sum;
}

double VarSupermoire::field(double x, double y) const {
    const double l1 = triLattice(x, y, angle1_, freq1_);
    const double l2 = triLattice(x * (1 + strain_), y * (1 - strain_), angle2_, freq2_);
    return l1 + l2 + mix_ * l1 * l2;
}

void VarSupermoire::calc() {
    *px += vvar * (*tx + scale_ * field(*tx, *ty));
    *py += vvar * (*ty + scale_ * field(*ty, *tx));
    *pz += vvar * *tz;
}

std::string VarSupermoire::variableNameAt(int index) const {
    switch (index) {
        case 0: return "supermoire_freq1";
        case 1: return "supermoire_freq2";
        case 2: return "supermoire_angle1";
        case 3: return "supermoire_angle2";
        case 4: return "supermoire_strain";
        case 5: return "supermoire_mix";
        case 6: return "supermoire_scale";
        default: return "";
    }
}

bool VarSupermoire::getVariable(const std::string& name, double& value) const {
    if (name == "supermoire_freq1") { value = freq1_; return true; }
    if (name == "supermoire_freq2") { value = freq2_; return true; }
    if (name == "supermoire_angle1") { value = angle1_; return true; }
    if (name == "supermoire_angle2") { value = angle2_; return true; }
    if (name == "supermoire_strain") { value = strain_; return true; }
    if (name == "supermoire_mix") { value = mix_; return true; }
    if (name == "supermoire_scale") { value = scale_; return true; }
    return false;
}

bool VarSupermoire::setVariable(const std::string& name, double& value) {
    if (name == "supermoire_freq1") { freq1_ = value; return true; }
    if (name == "supermoire_freq2") { freq2_ = value; return true; }
    if (name == "supermoire_angle1") { angle1_ = value; return true; }
    if (name == "supermoire_angle2") { angle2_ = value; return true; }
    if (name == "supermoire_strain") { strain_ = value; return true; }
    if (name == "supermoire_mix") { mix_ = value; return true; }
    if (name == "supermoire_scale") { scale_ = value; return true; }
    return false;
}

bool VarSupermoire::resetVariable(const std::string& name) {
    if (name == "supermoire_freq1") { freq1_ = 8; return true; }
    if (name == "supermoire_freq2") { freq2_ = 9; return true; }
    if (name == "supermoire_angle1") { angle1_ = 0; return true; }
    if (name == "supermoire_angle2") { angle2_ = 0.12; return true; }
    if (name == "supermoire_strain") { strain_ = 0; return true; }
    if (name == "supermoire_mix") { mix_ = 0.5; return true; }
    if (name == "supermoire_scale") { scale_ = 0.2; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarSupermoire>();
}

} // namespace apo
