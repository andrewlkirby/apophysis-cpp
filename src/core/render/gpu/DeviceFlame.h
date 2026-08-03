#pragma once

#include <array>
#include <vector>

#include "../../Flame.h"
#include "DeviceTypes.h"

// Host-side (plain C++, no CUDA syntax) upload package: everything
// GpuRenderer.cu needs to cudaMemcpy to device global memory before
// launching the chaos/tone-map kernels. Built once per render() call by
// buildDeviceFlame() below, which reuses the exact same camera/tone-map/
// xaos-table math the CPU renderer uses (RenderPlan.h) so the two backends
// can never silently drift apart on anything but the actual per-point
// variation math - see docs/GPU_RENDERING_PLAN.md.

namespace apo::gpu {

struct DeviceFlameHost {
    int numXForms = 0;
    bool useFinalXform = false;
    int finalXformIndex = -1; // index into `xforms` below, or -1

    std::vector<DeviceXform> xforms; // size numXForms (+1 if useFinalXform)
    std::vector<DeviceOp> ops;       // concatenated per-xform op sequences
    std::vector<double> params;      // concatenated per-op named-parameter values

    std::vector<int> propTable; // numXForms * propTableStride, row-major
    int propTableStride = 0;    // == apo::detail::kPropTableSize (RenderPlan.h) at build time

    DeviceProjectionKind projectionKind = DeviceProjectionKind::None;
    DeviceCameraMatrix cameraMatrix;
    double persp = 0, zpos = 0, dofCoef = 0;

    double camX0 = 0, camY0 = 0, camW = 1, camH = 1, bws = 1, bhs = 1;
    int bucketWidth = 0, bucketHeight = 0, oversample = 1, maxGutterWidth = 0;
    int numBatches = 1;

    std::vector<double> colorMap; // 256*3, row-major [i*3 + channel]

    int filterSize = 1;
    std::vector<double> filter; // filterSize*filterSize, row-major
    bool fastBucket = true;
    double k1 = 0, k2 = 0;
    std::array<double, 1025> logScale{};

    bool curvesSet = false;
    std::array<std::array<double, 257>, 4> curveTable{};

    bool adaptiveDE = false;
    double deMinRadius = 1.0, deMaxRadius = 1.0, deCurve = 0.2;
    int deMaxRadiusInt = 1;
    std::vector<double> deKernelsFlat;  // concatenated (2r+1)^2 kernels
    std::vector<int> deKernelOffsets;   // size deMaxRadiusInt+1, index 0 unused

    // Flame-level fields the tone-map kernel needs directly (not folded
    // into k1/k2/logScale/filter above) - matches what Renderer.cpp's CPU
    // toneMap() reads straight from `flame` rather than from RenderPlan.
    int width = 0, height = 0, channels = 3;
    double gamma = 1.0, gammaThreshold = 0.01, vibrancy = 1.0;
    std::array<int, 4> background{0, 0, 0, 0};
    bool transparency = false;
};

// Cheap eligibility scan (no allocation beyond what VariationRegistry
// lookups need) - true iff every xform (including the final xform, if
// used) has every nonzero-weight variation implemented on the GPU (see
// VariationKinds.h/.cpp). Used by RenderDispatcher to decide CPU vs GPU
// before attempting a build, and defensively re-checked by
// buildDeviceFlame() itself.
bool isFlameGpuEligible(const Flame& flame);

// Builds `out` from `flame`. Returns false (leaving `out` unspecified) if
// the flame isn't GPU-eligible or is degenerate (no xforms / zero-size
// canvas) - callers must fall back to the CPU renderer in that case.
bool buildDeviceFlame(const Flame& flame, DeviceFlameHost& out);

} // namespace apo::gpu
