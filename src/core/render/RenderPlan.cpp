#include "RenderPlan.h"

#include <algorithm>
#include <cmath>

#include "../Bezier.h"
#include "../XForm.h"

namespace apo::detail {

CameraMatrix buildCameraMatrix(double pitch, double yaw) {
    CameraMatrix m;
    m.m00 = std::cos(-yaw);
    m.m10 = -std::sin(-yaw);
    m.m20 = 0;
    m.m01 = std::cos(pitch) * std::sin(-yaw);
    m.m11 = std::cos(pitch) * std::cos(-yaw);
    m.m21 = -std::sin(pitch);
    m.m02 = std::sin(pitch) * std::sin(-yaw);
    m.m12 = std::sin(pitch) * std::cos(-yaw);
    m.m22 = std::cos(pitch);
    return m;
}

ProjectionKind selectProjectionKind(const Flame& flame) {
    if (flame.cameraDOF != 0) {
        return (flame.cameraYaw != 0) ? ProjectionKind::PitchYawDOF : ProjectionKind::PitchDOF;
    }
    if (flame.cameraPitch != 0 || flame.cameraYaw != 0) {
        return (flame.cameraYaw != 0) ? ProjectionKind::PitchYaw : ProjectionKind::Pitch;
    }
    return ProjectionKind::None;
}

std::vector<int> buildPropTable(const Flame& flame, int numXForms) {
    std::vector<int> table(static_cast<size_t>(numXForms) * kPropTableSize, 0);
    std::vector<double> tp(numXForms);

    for (int k = 0; k < numXForms; ++k) {
        double total = 0;
        for (int i = 0; i < numXForms; ++i) {
            tp[i] = flame.xform[i]->density * flame.xform[k]->modWeights[i];
            total += tp[i];
        }

        int* row = &table[static_cast<size_t>(k) * kPropTableSize];
        if (total > 0) {
            double loopValue = 0;
            for (int i = 0; i < kPropTableSize; ++i) {
                double propsum = 0;
                int j = -1;
                do {
                    ++j;
                    propsum += tp[j];
                } while (!(propsum > loopValue || j == numXForms - 1));
                row[i] = j;
                loopValue += total / kPropTableSize;
            }
        }
        // else: leave this row zero-filled (fallback to xform 0) - see the
        // original Renderer.cpp comment history for why this is a safe
        // simplification of the Pascal invalidXform placeholder.
    }
    return table;
}

void buildFilter(const Flame& flame, RenderPlan& plan) {
    const int oversample = plan.oversample;
    const int fw = static_cast<int>(2.0 * kFilterCutoff * oversample * flame.spatialFilterRadius);
    int filterSize = fw + 1;
    if ((filterSize + oversample) % 2 != 0) ++filterSize; // match oversample's parity

    const double adjust = (fw > 0) ? (kFilterCutoff * filterSize) / fw : 1.0;

    plan.filterSize = filterSize;
    plan.filter.assign(static_cast<size_t>(filterSize) * filterSize, 0.0);
    double sum = 0;
    for (int i = 0; i < filterSize; ++i) {
        for (int j = 0; j < filterSize; ++j) {
            const double ii = ((2.0 * i + 1.0) / filterSize - 1.0) * adjust;
            const double jj = ((2.0 * j + 1.0) / filterSize - 1.0) * adjust;
            const double v = std::exp(-2.0 * (ii * ii + jj * jj));
            plan.filter[static_cast<size_t>(i) * filterSize + j] = v;
            sum += v;
        }
    }
    if (sum != 0) {
        for (double& v : plan.filter) v /= sum;
    }
}

void buildBuffersAndCamera(const Flame& flame, RenderPlan& plan) {
    plan.oversample = flame.spatialOversample;
    plan.maxGutterWidth = (kMaxFilterWidth - plan.oversample) / 2;
    plan.gutterWidth = (plan.filterSize - plan.oversample) / 2;
    plan.bucketWidth = plan.oversample * flame.width + 2 * plan.maxGutterWidth;
    plan.bucketHeight = plan.oversample * flame.height + 2 * plan.maxGutterWidth;
    // ImageMaker.CreateImage recomputes "gutter_width" locally as
    // (FBucketWidth - FOversample*Width), which algebraically equals
    // 2*maxGutterWidth - i.e. the FastBucket check is really "does this
    // render's actual filter fit inside the worst-case-sized gutter the
    // buffer was allocated with", not the per-render gutterWidth above.
    plan.fastBucket = plan.filterSize <= plan.maxGutterWidth;

    const double scale = std::pow(2.0, flame.zoom);
    const double sampleDensity = flame.sampleDensity * scale * scale;
    const double ppux = flame.pixelsPerUnit * scale;
    const double ppuy = flame.pixelsPerUnit * scale;

    const double cornerX = flame.center[0] - flame.width / ppux / 2.0;
    const double cornerY = flame.center[1] - flame.height / ppuy / 2.0;
    const double t0 = plan.gutterWidth / (plan.oversample * ppux);
    const double t1 = plan.gutterWidth / (plan.oversample * ppuy);
    const double t2 = (2 * plan.maxGutterWidth - plan.gutterWidth) / (plan.oversample * ppux);
    const double t3 = (2 * plan.maxGutterWidth - plan.gutterWidth) / (plan.oversample * ppuy);

    plan.camX0 = cornerX - t0;
    plan.camY0 = cornerY - t1;
    const double camX1 = cornerX + flame.width / ppux + t2;
    const double camY1 = cornerY + flame.height / ppuy + t3;
    plan.camW = camX1 - plan.camX0;
    plan.camH = camY1 - plan.camY0;

    const double xSize = (std::fabs(plan.camW) > 0.01) ? (1.0 / plan.camW) : 1.0;
    const double ySize = (std::fabs(plan.camH) > 0.01) ? (1.0 / plan.camH) : 1.0;
    plan.bws = (plan.bucketWidth - 0.5) * xSize;
    plan.bhs = (plan.bucketHeight - 0.5) * ySize;

    // NSamples/NumBatches (RenderingInterface.pas's InitBuffers).
    const long long bucketCount = static_cast<long long>(plan.bucketWidth) * plan.bucketHeight;
    const double nSamples = std::round(sampleDensity * static_cast<double>(bucketCount) /
                                        (plan.oversample * plan.oversample));
    const int nbatches = std::max(1, flame.nbatches);
    plan.numBatches = std::max(1, static_cast<int>(std::round(nSamples / (nbatches * kSubBatchSize))));
}

void buildColorMap(const Flame& flame, RenderPlan& plan) {
    for (int i = 0; i < 256; ++i) {
        plan.colorMap[i][0] = static_cast<double>(flame.cmap.entries[i][0] * flame.whiteLevel) / 256.0;
        plan.colorMap[i][1] = static_cast<double>(flame.cmap.entries[i][1] * flame.whiteLevel) / 256.0;
        plan.colorMap[i][2] = static_cast<double>(flame.cmap.entries[i][2] * flame.whiteLevel) / 256.0;
    }
}

void buildToneMap(const Flame& flame, RenderPlan& plan) {
    const double scale2 = std::pow(2.0, flame.zoom);
    double sampleDensity = flame.sampleDensity * scale2 * scale2;
    if (sampleDensity == 0) sampleDensity = 0.001;

    plan.k1 = (flame.contrast * kBrightAdjust * flame.brightness * 268 * kPrefilterWhite) / 256.0;
    const double ppux = flame.pixelsPerUnit * scale2;
    const double ppuy = flame.pixelsPerUnit * scale2;
    const double area = static_cast<double>(flame.width) * flame.height / (ppux * ppuy);
    plan.k2 = (plan.oversample * plan.oversample) / (flame.contrast * area * flame.whiteLevel * sampleDensity);

    plan.logScale[0] = 0;
    for (int i = 1; i <= 1024; ++i) {
        plan.logScale[i] =
            (plan.k1 * std::log10(1 + flame.whiteLevel * i * plan.k2)) / (flame.whiteLevel * i);
    }
}

void buildCurveTables(const Flame& flame, RenderPlan& plan) {
    static const BezierCurve kIdentity{};
    plan.curvesSet = false;
    for (const auto& curve : flame.curves) {
        if (curve.points != kIdentity.points) {
            plan.curvesSet = true;
            break;
        }
    }
    if (!plan.curvesSet) return;

    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i <= 256; ++i) {
            plan.curveTable[c][i] = evalBezierCurve(flame.curves[c], i / 256.0) * 256.0;
        }
    }
}

void buildAdaptiveDE(const Flame& flame, RenderPlan& plan) {
    plan.adaptiveDE = flame.enableDE && flame.estimator > 0;
    if (!plan.adaptiveDE) return;

    const double cap = static_cast<double>(std::max(1, plan.maxGutterWidth));
    const double maxRadiusRaw = flame.estimator * plan.oversample + 1.0;
    const double minRadiusRaw = std::max(0.0, flame.estimatorMin) * plan.oversample + 1.0;
    plan.deMaxRadius = std::clamp(maxRadiusRaw, 1.0, cap);
    plan.deMinRadius = std::clamp(std::min(minRadiusRaw, plan.deMaxRadius), 1.0, cap);
    plan.deCurve = (flame.estimatorCurve > 0) ? flame.estimatorCurve : 0.2;

    plan.deMaxRadiusInt = std::max(1, static_cast<int>(std::ceil(plan.deMaxRadius)));
    plan.deKernels.assign(static_cast<size_t>(plan.deMaxRadiusInt) + 1, {});
    for (int r = 1; r <= plan.deMaxRadiusInt; ++r) {
        const int size = 2 * r + 1;
        std::vector<double> kernel(static_cast<size_t>(size) * size, 0.0);
        double sum = 0;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                const double di = (i - r) / static_cast<double>(r);
                const double dj = (j - r) / static_cast<double>(r);
                const double d2 = di * di + dj * dj;
                const double v = (d2 <= 1.0) ? std::exp(-2.0 * d2) : 0.0;
                kernel[static_cast<size_t>(i) * size + j] = v;
                sum += v;
            }
        }
        if (sum > 0) {
            for (double& v : kernel) v /= sum;
        }
        plan.deKernels[static_cast<size_t>(r)] = std::move(kernel);
    }
}

} // namespace apo::detail
