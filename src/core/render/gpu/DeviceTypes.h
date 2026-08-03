#pragma once

#include <cstdint>

// Plain, host/device-agnostic POD types - no CUDA-specific syntax (no
// __device__/__host__ qualifiers, no CUDA headers), so this file compiles
// identically whether included from a normal .cpp (DeviceFlameBuilder.cpp,
// RenderDispatcher.cpp) or a .cu translation unit compiled by nvcc. This is
// the "wire format" DeviceFlameBuilder (host, CPU-side) fills in and
// GpuRenderer.cu uploads verbatim via cudaMemcpy - see
// docs/GPU_RENDERING_PLAN.md's "DeviceFlame.h" section.

namespace apo::gpu {

// Must match apo::detail::kSubBatchSize/kPropTableSize (RenderPlan.h) and
// XForm.cpp's kFuse exactly - duplicated here (rather than including
// RenderPlan.h from device .cu code) because chaos_kernel.cu has no other
// reason to pull in Flame.h's full host-side object graph. numBatches
// itself (computed host-side by RenderPlan.h's buildBuffersAndCamera, using
// the same kSubBatchSize) is what ties the two together at render time -
// see DeviceFlameHost::numBatches.
constexpr int kDeviceSubBatchSize = 10000;
constexpr int kDeviceFuse = 15;

// GPU chaos-game mapping (see chaos_kernel.cu): the CPU renderer's
// renderBatch() draws a *fresh* random starting point and re-runs the FUSE
// warm-up on every single sub-batch call - only the RNG stream itself
// (ts.rng) persists across sub-batches within one CPU worker thread, not
// the point position. That means each sub-batch is already a fully
// independent unit of work, so the natural GPU mapping is one thread per
// sub-batch (not one thread per point, and not one persistent thread doing
// many sub-batches in sequence) - every thread seeds its own RNG stream
// from (renderSeed, its global sub-batch index), draws its own fresh point,
// FUSE-warms-up, iterates kDeviceSubBatchSize points, and atomicAdd's
// straight into one shared device-global histogram (no per-thread private
// bucket array the way the CPU path needs one - GPU atomics make that
// unnecessary). A render launches DeviceFlameHost::numBatches such threads
// total, chunked across multiple kernel launches for progress/cancel
// polling - see GpuRenderer.cu.

// Op-sequence "opcodes" a DeviceXform's variation list can contain, beyond
// the 0..28 local-variation ids (which match VariationRegistry's own fixed
// local-variation index space - see VariationKinds.h) and the
// kFirstRegisteredKind+N registered-variation ids. Mirrors the marker steps
// XForm::prepare() interleaves into calcFunctionList_ (precalcAngle/
// precalcSinCos/precalcAll/doPostTransform) - see XForm.cpp.
constexpr int kOpPrecalcAngle = 2000;
constexpr int kOpPrecalcSinCos = 2001;
constexpr int kOpPrecalcAll = 2002;
constexpr int kOpPostTransform = 2003;

// One step in an xform's per-point calc-function dispatch list (mirrors one
// entry of XForm::calcFunctionList_). `paramOffset` indexes into the
// DeviceFlame-wide `params` array (unused - 0 - for local variations and
// marker opcodes, which take no named parameters).
struct DeviceOp {
    std::int32_t kind = 0;
    std::int32_t paramOffset = 0;
    double vvar = 0;
};

// One xform's static (per-render-constant) data - the device analogue of a
// prepared XForm instance. `opsOffset`/`opsCount` slice into the
// DeviceFlame-wide `ops` array.
struct DeviceXform {
    double c00 = 1, c01 = 0, c10 = 0, c11 = 1, c20 = 0, c21 = 0; // affine
    double p00 = 1, p01 = 0, p10 = 0, p11 = 1, p20 = 0, p21 = 0; // post-transform affine
    std::uint8_t hasPostTransform = 0;
    double colorC1 = 1, colorC2 = 0;
    double transOpacity = 1;
    std::uint8_t opacityAlwaysPasses = 1;
    double pluginColor = 1;
    std::int32_t opsOffset = 0;
    std::int32_t opsCount = 0;
};

enum class DeviceProjectionKind : std::int32_t { None = 0, Pitch, PitchYaw, PitchDOF, PitchYawDOF };

struct DeviceCameraMatrix {
    double m00 = 1, m10 = 0, m20 = 0;
    double m01 = 0, m11 = 1, m21 = 0;
    double m02 = 0, m12 = 0, m22 = 1;
};

// One 4-channel histogram bucket - same layout/semantics as Renderer.cpp's
// BucketT<double>, accumulated via atomicAdd from every GPU thread. Two
// scalar widths, matching the CPU renderer's own BucketPrecision option
// (Renderer.h): Double (DeviceBucket) is every existing caller's default;
// Float (DeviceBucketF) halves the working set/atomic-write traffic of the
// chaos game's random-access bucket writes - the classic flame-fractal
// cache-miss bottleneck - at the same float::count-saturation-past-~16.7M-
// points tradeoff the CPU side already documents for its own Float path.
// Only one of DeviceKernelParams::bucketsD/bucketsF is ever non-null for a
// given render, selected by useFloatBuckets - see GpuRenderer.cu.
struct DeviceBucket {
    double red = 0, green = 0, blue = 0, count = 0;
};
struct DeviceBucketF {
    float red = 0, green = 0, blue = 0, count = 0;
};

// Everything chaos_kernel.cu's per-sub-batch kernel needs, bundled into one
// by-value kernel parameter (well under CUDA's parameter-space limit) plus
// a handful of device-global-memory pointers uploaded once per render by
// GpuRenderer.cu - see DeviceFlameHost (DeviceFlame.h) for the host-side
// package this is built from.
struct DeviceKernelParams {
    int numXForms = 0;
    int finalXformIndex = -1; // -1 if the flame has no (or a disabled) final xform
    int propTableStride = 0;  // == DeviceFlameHost::propTableStride

    DeviceProjectionKind projectionKind = DeviceProjectionKind::None;
    DeviceCameraMatrix cameraMatrix;
    double persp = 0, zpos = 0, dofCoef = 0;

    double camX0 = 0, camY0 = 0, camW = 1, camH = 1, bws = 1, bhs = 1;
    int bucketWidth = 0, bucketHeight = 0;

    const DeviceXform* xforms = nullptr;
    const DeviceOp* ops = nullptr;
    const double* varParams = nullptr;
    const int* propTable = nullptr;
    const double* colorMap = nullptr; // 256*3, row-major [i*3 + channel]
    std::uint8_t useFloatBuckets = 0; // selects bucketsF (nonzero) or bucketsD (zero) below
    DeviceBucket* bucketsD = nullptr;
    DeviceBucketF* bucketsF = nullptr;
};

// Everything tonemap_kernel.cu's per-pixel kernel needs. Large fixed-size
// CPU-side tables (RenderPlan's 1025-entry logScale, 4x257-entry
// curveTable, the filter/adaptive-DE kernels) are passed as device-global
// pointers, not by value - unlike DeviceKernelParams above, several of
// these would blow well past CUDA's ~4KB kernel-parameter budget if
// embedded directly (logScale alone is 1025 doubles / 8.2KB).
struct DeviceToneMapParams {
    int width = 0, height = 0, channels = 3;
    int oversample = 1, maxGutterWidth = 0;
    int bucketWidth = 0, bucketHeight = 0;
    int filterSize = 1;
    std::uint8_t fastBucket = 1;
    double k1 = 0, k2 = 0;
    double whiteLevel = 200; // Flame::whiteLevel (int) widened to double once, at build time
    double gamma = 1.0, gammaThreshold = 0.01, vibrancy = 1.0;
    int background[4] = {0, 0, 0, 0};
    std::uint8_t transparency = 0;
    std::uint8_t curvesSet = 0;
    std::uint8_t adaptiveDE = 0;
    double deMinRadius = 1.0, deMaxRadius = 1.0, deCurve = 0.2;
    int deMaxRadiusInt = 1;

    const double* filter = nullptr;        // filterSize*filterSize, row-major
    const double* logScale = nullptr;      // 1025 entries
    const double* curveTable = nullptr;    // 4*257, row-major [channel*257 + i]
    const double* deKernelsFlat = nullptr; // concatenated (2r+1)^2 kernels
    const int* deKernelOffsets = nullptr;  // size deMaxRadiusInt+1
    std::uint8_t useFloatBuckets = 0; // selects bucketsF (nonzero) or bucketsD (zero) below
    const DeviceBucket* bucketsD = nullptr;
    const DeviceBucketF* bucketsF = nullptr;
    unsigned char* pixels = nullptr; // width*height*channels output
};

} // namespace apo::gpu
