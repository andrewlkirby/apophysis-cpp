#include "FlameOps.h"

#include <cstddef>

namespace apo {

void randomizeXformWeights(Flame& flame, Rng& rng) {
    const int n = flame.numXForms();
    for (int i = 0; i < n; ++i) flame.xform[static_cast<size_t>(i)]->density = rng.uniform01();
}

void equalizeXformWeights(Flame& flame) {
    const int n = flame.numXForms();
    for (int i = 0; i < n; ++i) flame.xform[static_cast<size_t>(i)]->density = 0.5;
}

void calculateXformColorValues(Flame& flame) {
    const int n = flame.numXForms();
    if (n <= 1) return;
    for (int i = 0; i < n; ++i) {
        flame.xform[static_cast<size_t>(i)]->color = static_cast<double>(i) / static_cast<double>(n - 1);
    }
}

void randomizeXformColorValues(Flame& flame, Rng& rng) {
    const int n = flame.numXForms();
    for (int i = 0; i < n; ++i) flame.xform[static_cast<size_t>(i)]->color = rng.uniform01();
}

} // namespace apo
