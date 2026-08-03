#pragma once

#include <array>
#include <vector>

#include "../Flame.h"

namespace apo::detail {

// Everything in this header/RenderPlan.cpp was extracted verbatim (no
// behavior change) out of Renderer.cpp's anonymous namespace so the GPU
// render backend's DeviceFlame builder (src/core/render/gpu/) can reuse the
// exact same camera/tone-map/xaos-table math the CPU path uses, rather than
// maintaining a second, hand-copied formula set that could silently drift -
// see docs/GPU_RENDERING_PLAN.md's "Key existing code to reuse" section.
// Renderer.cpp includes this header and pulls these names in via `using
// namespace apo::detail;` in its own anonymous namespace, so this move is
// purely mechanical from the CPU renderer's point of view.
//
// Deliberately namespace apo::detail, not anonymous: these functions need
// external linkage so a second translation unit (the GPU builder) can call
// them, but the name signals "internal plumbing, not public API" the same
// way the anonymous namespace did before.

constexpr int kSubBatchSize = 10000;
constexpr int kPropTableSize = 1024;
constexpr int kMaxFilterWidth = 25;
constexpr double kFilterCutoff = 1.8;
constexpr double kBrightAdjust = 2.3;
constexpr double kPrefilterWhite = static_cast<double>(1 << 26);

enum class ProjectionKind { None, Pitch, PitchYaw, PitchDOF, PitchYawDOF };

// CameraMatrix[a,b] in Pascal (ControlPoint.pas's 3x3 CameraMatrix) - named
// fields instead of a 2D array so the projection formulas read the same as
// the Pascal source line-for-line.
struct CameraMatrix {
    double m00 = 1, m10 = 0, m20 = 0;
    double m01 = 0, m11 = 1, m21 = 0;
    double m02 = 0, m12 = 0, m22 = 1;
};

CameraMatrix buildCameraMatrix(double pitch, double yaw);
ProjectionKind selectProjectionKind(const Flame& flame);

// Proportional xform-selection table: propTable[k*kPropTableSize + slot] is
// the index of the xform to jump to next, given the chain is currently at
// xform k, weighted by density*modWeights (xaos). Matches ControlPoint.pas's
// Prepare() PropTable build exactly.
std::vector<int> buildPropTable(const Flame& flame, int numXForms);

// Prepared once per render, shared (read-only) across every CPU worker
// thread, and - via the GPU builder - serialized into DeviceFlame's upload
// arrays for the GPU backend.
struct RenderPlan {
    int numXForms = 0;
    bool useFinalXform = false;
    std::vector<int> propTable;

    ProjectionKind projectionKind = ProjectionKind::None;
    CameraMatrix cameraMatrix;
    double persp = 0, zpos = 0, dofCoef = 0;

    double camX0 = 0, camY0 = 0, camW = 1, camH = 1;
    double bws = 1, bhs = 1;
    int bucketWidth = 0, bucketHeight = 0;
    int oversample = 1;
    int gutterWidth = 0, maxGutterWidth = 0;

    std::array<std::array<double, 3>, 256> colorMap{};

    int numBatches = 1;

    // Tone-mapping (ImageMaker.pas's CreateFilter/CreateImage) - computed
    // here too since filter size feeds gutter/buffer sizing above.
    int filterSize = 1;
    std::vector<double> filter; // filterSize*filterSize, row-major
    bool fastBucket = true;
    double k1 = 0, k2 = 0;
    std::array<double, 1025> logScale{};

    // Bezier tone curves (see buildCurveTables) - index 0 is the master/
    // "All" channel, 1/2/3 are Red/Green/Blue, each a 257-entry (0..256
    // inclusive) lookup table. curvesSet mirrors ImageMaker.pas's own
    // check: false (skip curve application entirely) only while every
    // channel is still at its exact default identity configuration.
    bool curvesSet = false;
    std::array<std::array<double, 257>, 4> curveTable{};

    // Adaptive density estimation - deKernels[r] (r=1..deMaxRadiusInt) is a
    // normalized (2r+1)x(2r+1) circular Gaussian kernel, row-major; index 0
    // is unused (the minimum meaningful radius is 1).
    bool adaptiveDE = false;
    double deMinRadius = 1.0, deMaxRadius = 1.0, deCurve = 0.2;
    int deMaxRadiusInt = 1;
    std::vector<std::vector<double>> deKernels;
};

// Matches ImageMaker.pas's CreateFilter exactly: a normalized Gaussian-ish
// kernel, sized from spatialOversample and spatialFilterRadius.
void buildFilter(const Flame& flame, RenderPlan& plan);

// Matches RenderingInterface.pas's CalcBufferSize + CreateCamera exactly.
// Must be called after buildFilter() (reads plan.filterSize).
void buildBuffersAndCamera(const Flame& flame, RenderPlan& plan);

// Matches RenderingInterface.pas's CreateColorMap exactly: the 256-entry
// palette scaled by white_level, precomputed once per render.
void buildColorMap(const Flame& flame, RenderPlan& plan);

// Matches ImageMaker.pas's CreateImage tone-map setup exactly (k1/k2 and the
// 1025-entry log-scale lookup table). Must be called after
// buildBuffersAndCamera() (reads plan.oversample).
void buildToneMap(const Flame& flame, RenderPlan& plan);

// Matches ImageMaker.pas's csa[] lookup-table construction and its
// curvesSet detection.
void buildCurveTables(const Flame& flame, RenderPlan& plan);

// Precomputes buildAdaptiveDE's per-integer-radius kernels and the radius
// bounds a per-pixel density maps into. Must be called after
// buildBuffersAndCamera() (reads plan.maxGutterWidth).
void buildAdaptiveDE(const Flame& flame, RenderPlan& plan);

} // namespace apo::detail
