#pragma once

#include <algorithm>

namespace apo::ui {

// Fits a preview render into the space available (availW x availH) while
// preserving the flame's own declared width:height aspect ratio, rather
// than just handing the widget's raw pixel dimensions to
// Flame::adjustScale(). adjustScale() derives pixelsPerUnit purely from the
// width ratio and then blindly assigns height - if the target w:h doesn't
// match the flame's own width:height, the render comes out visibly
// stretched/squished (e.g. a widescreen flame rendered into a square-ish
// panel looks squashed into a square). Mirrors the fit MainWindow's own
// camera-preview path already applies to previewLabel_.
inline void fitPreviewSize(int availW, int availH, int flameW, int flameH, int& outW, int& outH) {
    const int aw = std::max(availW, 1);
    const int ah = std::max(availH, 1);
    const double aspect = flameH > 0 ? static_cast<double>(flameW) / flameH : 1.0;
    outW = aw;
    outH = ah;
    if (static_cast<double>(aw) / ah > aspect) {
        outW = std::max(1, static_cast<int>(ah * aspect));
    } else {
        outH = std::max(1, static_cast<int>(aw / aspect));
    }
}

} // namespace apo::ui
