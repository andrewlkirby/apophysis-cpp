#include "VarAuger.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarAuger::calc() {
    const double x = *tx;
    const double y = *ty;

    const double s = std::sin(augerFreq_ * x);
    const double t = std::sin(augerFreq_ * y);

    const double dx = x + augerWeight_ * (0.5 * augerScale_ * t + std::fabs(x) * t);
    const double dy = y + augerWeight_ * (0.5 * augerScale_ * s + std::fabs(y) * s);

    *px += vvar * (x + augerSym_ * (dx - x));
    *py += vvar * dy;
    *pz += vvar * *tz;
}

std::string VarAuger::variableNameAt(int index) const {
    switch (index) {
        case 0: return "auger_freq";
        case 1: return "auger_weight";
        case 2: return "auger_scale";
        case 3: return "auger_sym";
        default: return "";
    }
}

bool VarAuger::getVariable(const std::string& name, double& value) const {
    if (name == "auger_freq") { value = augerFreq_; return true; }
    if (name == "auger_weight") { value = augerWeight_; return true; }
    if (name == "auger_scale") { value = augerScale_; return true; }
    if (name == "auger_sym") { value = augerSym_; return true; }
    return false;
}

bool VarAuger::setVariable(const std::string& name, double& value) {
    if (name == "auger_freq") { augerFreq_ = value; return true; }
    if (name == "auger_weight") { augerWeight_ = value; return true; }
    if (name == "auger_scale") { augerScale_ = value; return true; }
    if (name == "auger_sym") { augerSym_ = value; return true; }
    return false;
}

bool VarAuger::resetVariable(const std::string& name) {
    // Matches TVariationAuger.ResetVariable exactly, including its existing
    // typo: resetting "auger_scale" leaves augerScale_ untouched and instead
    // resets augerSym_ to 0.1 (the Pascal source writes `auger_sym := 0.1`
    // inside the `auger_scale` branch) - preserved here rather than fixed,
    // since it's the shipped app's actual behavior.
    if (name == "auger_freq") { augerFreq_ = 5; return true; }
    if (name == "auger_weight") { augerWeight_ = 0.5; return true; }
    if (name == "auger_scale") { augerSym_ = 0.1; return true; }
    if (name == "auger_sym") { augerSym_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarAuger>();
}

} // namespace apo
