#include "Symmetry.h"

#include <cmath>

#include "../Rng.h"

namespace apo {

int addSymmetry(Flame& flame, int sym) {
    if (sym == 0 || sym == 1) return 0;

    int i = flame.numXForms();
    if (i >= Flame::kMaxXForms) return 0;

    flame.symmetry = sym;
    int added = 0;

    if (sym < 0) {
        XForm& xf = *flame.xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.symmetry = 1;
        xf.color = 1.0;
        xf.c[0] = {-1.0, 0.0};
        xf.c[1] = {0.0, 1.0};
        xf.c[2] = {0.0, 0.0};
        ++i;
        ++added;
        sym = -sym;
    }

    const double a = kTwoPi / sym;
    for (int k = 1; k < sym && i < Flame::kMaxXForms; ++k) {
        XForm& xf = *flame.xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.symmetry = 1;

        if (sym < 3) {
            xf.color = 0.0;
        } else {
            double c = static_cast<double>(k - 1) / (sym - 2);
            while (c > 1.0) c -= 1.0; // matches the original's defensive clamp
            xf.color = c;
        }

        xf.c[0] = {std::cos(k * a), std::sin(k * a)};
        xf.c[1] = {-xf.c[0][1], xf.c[0][0]};
        xf.c[2] = {0.0, 0.0};

        ++i;
        ++added;
    }

    return added;
}

} // namespace apo
