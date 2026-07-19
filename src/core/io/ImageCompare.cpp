#include "ImageCompare.h"

#include <cmath>
#include <cstddef>

namespace apo {

double computePsnr(int width, int height, int channels, const std::uint8_t* a, const std::uint8_t* b) {
    if (width <= 0 || height <= 0 || channels <= 0 || a == nullptr || b == nullptr) return -1;

    const size_t n = static_cast<size_t>(width) * height * channels;
    double sumSquaredError = 0;
    for (size_t i = 0; i < n; ++i) {
        const double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        sumSquaredError += diff * diff;
    }

    if (sumSquaredError == 0) return std::numeric_limits<double>::infinity();

    const double mse = sumSquaredError / static_cast<double>(n);
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

} // namespace apo
