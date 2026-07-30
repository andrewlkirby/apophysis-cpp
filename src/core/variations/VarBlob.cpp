#include "VarBlob.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarBlob::calc() {
    const double rad = std::sqrt(*tx * *tx + *ty * *ty);
    const double angle = std::atan2(*tx, *ty);
    const double bdiff = blobHigh_ - blobLow_;

    const double r = rad * (blobLow_ + bdiff * (0.5 + 0.5 * std::sin(blobWaves_ * angle)));
    const double sina = *tx / rad;
    const double cosa = *ty / rad;

    *px += vvar * sina * r;
    *py += vvar * cosa * r;
    *pz += vvar * *tz;
}

std::string VarBlob::variableNameAt(int index) const {
    switch (index) {
        case 0: return "blob_low";
        case 1: return "blob_high";
        case 2: return "blob_waves";
        default: return "";
    }
}

bool VarBlob::getVariable(const std::string& name, double& value) const {
    if (name == "blob_low") { value = blobLow_; return true; }
    if (name == "blob_high") { value = blobHigh_; return true; }
    if (name == "blob_waves") { value = blobWaves_; return true; }
    return false;
}

bool VarBlob::setVariable(const std::string& name, double& value) {
    if (name == "blob_low") { blobLow_ = value; return true; }
    if (name == "blob_high") { blobHigh_ = value; return true; }
    if (name == "blob_waves") { blobWaves_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarBlob>();
}

} // namespace apo
