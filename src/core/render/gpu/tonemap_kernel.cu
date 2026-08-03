#include "tonemap_kernel.cuh"

#include <cmath>

namespace apo::gpu {

namespace {

constexpr double kDevicePrefilterWhite = static_cast<double>(1 << 26); // matches RenderPlan.h's kPrefilterWhite

// Plain double 4-tuple every bucket read promotes into immediately, so
// every other line of tonemapKernel below stays exactly as it was before
// float-bucket support existed - only readBucketAt/devSafeGetBucket know
// about DeviceToneMapParams::useFloatBuckets at all. tp.useFloatBuckets is
// a per-render constant, so this branch is uniform across every thread in
// the kernel - no warp divergence, just picks which array gets read from
// for the whole launch.
struct Bucket4 {
    double red, green, blue, count;
};

__device__ Bucket4 devReadBucketAt(const DeviceToneMapParams& tp, size_t idx) {
    if (tp.useFloatBuckets) {
        const DeviceBucketF& b = tp.bucketsF[idx];
        return Bucket4{b.red, b.green, b.blue, b.count};
    }
    const DeviceBucket& b = tp.bucketsD[idx];
    return Bucket4{b.red, b.green, b.blue, b.count};
}

__device__ Bucket4 devSafeGetBucket(const DeviceToneMapParams& tp, int x, int y) {
    x = min(max(x, 0), tp.bucketWidth - 1);
    y = min(max(y, 0), tp.bucketHeight - 1);
    return devReadBucketAt(tp, static_cast<size_t>(y) * tp.bucketWidth + x);
}

__device__ double devLogScaleFor(const DeviceToneMapParams& tp, double count) {
    if (count < 1024) return tp.logScale[static_cast<int>(llround(count))];
    return (tp.k1 * log10(1 + tp.whiteLevel * count * tp.k2)) / (tp.whiteLevel * count);
}

__device__ double devAdaptiveRadiusFor(const DeviceToneMapParams& tp, double count) {
    const double span = tp.deMaxRadius - tp.deMinRadius;
    if (span <= 0) return tp.deMaxRadius;
    const double t = 1.0 / pow(1.0 + max(0.0, count), tp.deCurve);
    return tp.deMinRadius + span * t;
}

// tp.curveTable is row-major [channel*257 + i] - channel 0 is the master/
// "All" curve, applied first; 1/2/3 are Red/Green/Blue. Matches
// Renderer.cpp's toneMap() applyCurve lambda exactly (including the
// master-index clamp - see that lambda's own comment on why this differs
// from the original Pascal's unchecked array read).
__device__ int devApplyCurve(const DeviceToneMapParams& tp, int channel, int raw) {
    if (raw < 0 || raw > 256) return raw;
    const int masterIdx = min(max(static_cast<int>(llround(tp.curveTable[raw])), 0), 256);
    return static_cast<int>(llround(tp.curveTable[channel * 257 + masterIdx]));
}

} // namespace

__global__ void tonemapKernel(DeviceToneMapParams tp) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    const int total = tp.width * tp.height;
    if (idx >= total) return;
    const int row = idx / tp.width;
    const int col = idx % tp.width;

    const double gammaExp = (tp.gamma == 0) ? 0 : (1.0 / tp.gamma);
    const double funcVal = (tp.gammaThreshold != 0) ? pow(tp.gammaThreshold, gammaExp - 1) : 0;
    const int vib = static_cast<int>(llround(tp.vibrancy * 256));
    const int notVib = 256 - vib;

    const int by = row * tp.oversample + tp.maxGutterWidth;
    const int bx = col * tp.oversample + tp.maxGutterWidth;

    double fp[4] = {0, 0, 0, 0};
    if (tp.filterSize > 1) {
        for (int ii = 0; ii < tp.filterSize; ++ii) {
            for (int jj = 0; jj < tp.filterSize; ++jj) {
                const double filterValue = tp.filter[static_cast<size_t>(ii) * tp.filterSize + jj];
                const Bucket4 bucket =
                    tp.fastBucket ? devReadBucketAt(tp, static_cast<size_t>(by + ii) * tp.bucketWidth + bx + jj)
                                  : devSafeGetBucket(tp, bx + jj, by + ii);
                const double ls = devLogScaleFor(tp, bucket.count);
                fp[0] += filterValue * ls * bucket.red;
                fp[1] += filterValue * ls * bucket.green;
                fp[2] += filterValue * ls * bucket.blue;
                fp[3] += filterValue * ls * bucket.count;
            }
        }
        fp[0] /= kDevicePrefilterWhite;
        fp[1] /= kDevicePrefilterWhite;
        fp[2] /= kDevicePrefilterWhite;
        fp[3] = tp.whiteLevel * fp[3] / kDevicePrefilterWhite;
    } else {
        const Bucket4 bucket = tp.fastBucket ? devReadBucketAt(tp, static_cast<size_t>(by) * tp.bucketWidth + bx)
                                              : devSafeGetBucket(tp, bx, by);
        const double ls = devLogScaleFor(tp, bucket.count) / kDevicePrefilterWhite;
        fp[0] = ls * bucket.red;
        fp[1] = ls * bucket.green;
        fp[2] = ls * bucket.blue;
        fp[3] = ls * bucket.count * tp.whiteLevel;
    }

    if (tp.adaptiveDE && fp[3] > 0) {
        const Bucket4 centerBucket = tp.fastBucket
                                          ? devReadBucketAt(tp, static_cast<size_t>(by) * tp.bucketWidth + bx)
                                          : devSafeGetBucket(tp, bx, by);
        const double radius = devAdaptiveRadiusFor(tp, centerBucket.count);
        const int r = min(max(static_cast<int>(llround(radius)), 1), tp.deMaxRadiusInt);
        const int kernelOffset = tp.deKernelOffsets[r];
        const int size = 2 * r + 1;

        double dp[4] = {0, 0, 0, 0};
        for (int ii = 0; ii < size; ++ii) {
            for (int jj = 0; jj < size; ++jj) {
                const double kv = tp.deKernelsFlat[kernelOffset + ii * size + jj];
                if (kv == 0) continue;
                const Bucket4 bucket = devSafeGetBucket(tp, bx + jj - r, by + ii - r);
                const double ls = devLogScaleFor(tp, bucket.count);
                dp[0] += kv * ls * bucket.red;
                dp[1] += kv * ls * bucket.green;
                dp[2] += kv * ls * bucket.blue;
                dp[3] += kv * ls * bucket.count;
            }
        }
        fp[0] = dp[0] / kDevicePrefilterWhite;
        fp[1] = dp[1] / kDevicePrefilterWhite;
        fp[2] = dp[2] / kDevicePrefilterWhite;
        fp[3] = tp.whiteLevel * dp[3] / kDevicePrefilterWhite;
    }

    int r = 0, g = 0, b = 0, a = 0;
    if (fp[3] > 0) {
        double alpha;
        if (fp[3] <= tp.gammaThreshold) {
            const double frac = fp[3] / tp.gammaThreshold;
            alpha = (1 - frac) * fp[3] * funcVal + frac * pow(fp[3], gammaExp);
        } else {
            alpha = pow(fp[3], gammaExp);
        }

        const double ls = vib * alpha / fp[3];
        a = min(max(static_cast<int>(llround(alpha * 256)), 0), 255);
        r = static_cast<int>(llround(ls * fp[0] + notVib * pow(fp[0], gammaExp)));
        g = static_cast<int>(llround(ls * fp[1] + notVib * pow(fp[1], gammaExp)));
        b = static_cast<int>(llround(ls * fp[2] + notVib * pow(fp[2], gammaExp)));

        if (tp.curvesSet) {
            r = devApplyCurve(tp, 1, r);
            g = devApplyCurve(tp, 2, g);
            b = devApplyCurve(tp, 3, b);
        }

        if (!tp.transparency) {
            r += ((255 - a) * tp.background[0]) >> 8;
            g += ((255 - a) * tp.background[1]) >> 8;
            b += ((255 - a) * tp.background[2]) >> 8;
        } else if (a > 0) {
            r = r * 255 / a;
            g = g * 255 / a;
            b = b * 255 / a;
        }
        r = min(max(r, 0), 255);
        g = min(max(g, 0), 255);
        b = min(max(b, 0), 255);
    } else if (!tp.transparency) {
        r = tp.background[0];
        g = tp.background[1];
        b = tp.background[2];
        a = 255;
    }

    const size_t pixelOffset = (static_cast<size_t>(row) * tp.width + col) * tp.channels;
    tp.pixels[pixelOffset + 0] = static_cast<unsigned char>(r);
    tp.pixels[pixelOffset + 1] = static_cast<unsigned char>(g);
    tp.pixels[pixelOffset + 2] = static_cast<unsigned char>(b);
    if (tp.channels == 4) tp.pixels[pixelOffset + 3] = static_cast<unsigned char>(a);
}

} // namespace apo::gpu
