#include "VariationRegistry.h"

#include <stdexcept>

namespace apo {

VariationRegistry& VariationRegistry::instance() {
    static VariationRegistry registry;
    return registry;
}

const std::array<std::string, VariationRegistry::kNumLocalVars>& VariationRegistry::localVarNames() {
    // Matches XFormMan.pas's cvarnames exactly, including the fact that the
    // 'Square' procedure's variable name is 'diamond' (index 10) - that's
    // the original naming, not a typo introduced here.
    static const std::array<std::string, kNumLocalVars> names = {
        "linear",
        "flatten",
        "sinusoidal",
        "spherical",
        "swirl",
        "horseshoe",
        "polar",
        "disc",
        "spiral",
        "hyperbolic",
        "diamond",
        "eyefish",
        "bubble",
        "cylinder",
        "noise",
        "blur",
        "gaussian_blur",
        "zblur",
        "blur3D",
        "pre_blur",
        "pre_zscale",
        "pre_ztranslate",
        "pre_rotate_x",
        "pre_rotate_y",
        "zscale",
        "ztranslate",
        "zcone",
        "post_rotate_x",
        "post_rotate_y",
    };
    return names;
}

void VariationRegistry::registerVariation(std::unique_ptr<VariationFactory> factory) {
    const int newVariationIndex = kNumLocalVars + static_cast<int>(factories_.size());
    const int n = factory->numVariables();
    for (int i = 0; i < n; ++i) {
        variableNames_.push_back(factory->variableNameAt(i));
        variableNameToVariationIndex_.push_back(newVariationIndex);
    }
    factories_.push_back(std::move(factory));
}

std::string VariationRegistry::varName(int index) const {
    if (index < kNumLocalVars) {
        return localVarNames()[index];
    }
    return factories_.at(index - kNumLocalVars)->name();
}

int VariationRegistry::variationIndex(const std::string& name) const {
    for (int i = nrVar() - 1; i >= 0; --i) {
        if (varName(i) == name) return i;
    }
    return -1;
}

int VariationRegistry::variationIndexFromVariableNameIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(variableNameToVariationIndex_.size())) {
        return -1;
    }
    return variableNameToVariationIndex_[index];
}

} // namespace apo
