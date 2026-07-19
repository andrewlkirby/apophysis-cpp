#include "VarPDJ.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

VarPDJ::VarPDJ()
    : a_(6 * constructionRandom01() - 3),
      b_(6 * constructionRandom01() - 3),
      c_(6 * constructionRandom01() - 3),
      d_(6 * constructionRandom01() - 3) {}

void VarPDJ::calc() {
    *px += vvar * (std::sin(a_ * *ty) - std::cos(b_ * *tx));
    *py += vvar * (std::sin(c_ * *tx) - std::cos(d_ * *ty));
    *pz += vvar * *tz;
}

std::string VarPDJ::variableNameAt(int index) const {
    switch (index) {
        case 0: return "pdj_a";
        case 1: return "pdj_b";
        case 2: return "pdj_c";
        case 3: return "pdj_d";
        default: return "";
    }
}

bool VarPDJ::getVariable(const std::string& name, double& value) const {
    if (name == "pdj_a") { value = a_; return true; }
    if (name == "pdj_b") { value = b_; return true; }
    if (name == "pdj_c") { value = c_; return true; }
    if (name == "pdj_d") { value = d_; return true; }
    return false;
}

bool VarPDJ::setVariable(const std::string& name, double& value) {
    if (name == "pdj_a") { a_ = value; return true; }
    if (name == "pdj_b") { b_ = value; return true; }
    if (name == "pdj_c") { c_ = value; return true; }
    if (name == "pdj_d") { d_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPDJ>();
}

} // namespace apo
