#include "DeviceFlame.h"

#include <string>

#include "../../VariationRegistry.h"
#include "../../XForm.h"
#include "../RenderPlan.h"
#include "VariationKinds.h"

namespace apo::gpu {

namespace {

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// -1 if variation index `varIndex` (0..registry.nrVar()-1) has no GPU
// implementation; otherwise its device op kind (0..28 for locals, unmapped
// - see VariationKinds.h - or kFirstRegisteredKind+N for a registered one).
int deviceKindForVariationIndex(const VariationRegistry& registry, int varIndex) {
    if (varIndex < kNumLocalVars) return varIndex;
    const RegisteredVarInfo* info = lookupRegisteredVarKind(registry.varName(varIndex));
    return info ? info->kind : -1;
}

bool isXFormGpuEligible(const XForm& xform, const VariationRegistry& registry) {
    const int nrVar = registry.nrVar();
    for (int i = 0; i < nrVar; ++i) {
        if (xform.variation(i) == 0.0) continue;
        if (deviceKindForVariationIndex(registry, i) < 0) return false;
    }
    return true;
}

// Appends `xform`'s per-point op sequence to `ops`/`params`, filling in
// `dx.opsOffset`/`opsCount`/`hasPostTransform` - mirrors XForm::prepare()'s
// calcFunctionList_ construction exactly (see XForm.cpp): pre_ variations
// first, a precalc marker if polar/disc or spiral/diamond are weighted,
// then normal variations, then post_/flatten variations, then a
// post-transform step if the xform's `p` affine differs from identity.
void appendXformOps(const XForm& xform, const VariationRegistry& registry, std::vector<DeviceOp>& ops,
                     std::vector<double>& params, DeviceXform& dx) {
    const int nrVar = registry.nrVar();
    const bool calculateAngle = (xform.variation(6) != 0.0) || (xform.variation(7) != 0.0);
    const bool calculateSinCos = (xform.variation(8) != 0.0) || (xform.variation(10) != 0.0);

    auto pushVarOp = [&](int i) {
        DeviceOp op;
        op.vvar = xform.variation(i);
        if (i < kNumLocalVars) {
            op.kind = i;
            op.paramOffset = 0;
        } else {
            const RegisteredVarInfo* info = lookupRegisteredVarKind(registry.varName(i));
            // Guaranteed non-null: buildDeviceFlame() already rejected any
            // flame containing an unsupported weighted variation before
            // this function is ever called.
            op.kind = info->kind;
            op.paramOffset = static_cast<int>(params.size());
            for (const std::string& pname : info->paramNames) {
                double v = 0;
                xform.getVariable(pname, v);
                params.push_back(v);
            }
        }
        ops.push_back(op);
    };

    const auto opsStart = static_cast<std::int32_t>(ops.size());

    for (int i = 0; i < nrVar; ++i) {
        if (xform.variation(i) != 0.0 && startsWith(registry.varName(i), "pre_")) pushVarOp(i);
    }

    if (calculateAngle && calculateSinCos) {
        ops.push_back(DeviceOp{kOpPrecalcAll, 0, 0});
    } else if (calculateAngle) {
        ops.push_back(DeviceOp{kOpPrecalcAngle, 0, 0});
    } else if (calculateSinCos) {
        ops.push_back(DeviceOp{kOpPrecalcSinCos, 0, 0});
    }

    for (int i = 0; i < nrVar; ++i) {
        if (xform.variation(i) == 0.0) continue;
        const std::string name = registry.varName(i);
        if (startsWith(name, "pre_") || startsWith(name, "post_") || name == "flatten") continue;
        pushVarOp(i);
    }

    for (int i = 0; i < nrVar; ++i) {
        if (xform.variation(i) == 0.0) continue;
        const std::string name = registry.varName(i);
        if (startsWith(name, "post_") || name == "flatten") pushVarOp(i);
    }

    const bool hasPost = (xform.p[0][0] != 1) || (xform.p[0][1] != 0) || (xform.p[1][0] != 0) ||
                          (xform.p[1][1] != 1) || (xform.p[2][0] != 0) || (xform.p[2][1] != 0);
    if (hasPost) {
        ops.push_back(DeviceOp{kOpPostTransform, 0, 0});
        dx.p00 = xform.p[0][0];
        dx.p01 = xform.p[0][1];
        dx.p10 = xform.p[1][0];
        dx.p11 = xform.p[1][1];
        dx.p20 = xform.p[2][0];
        dx.p21 = xform.p[2][1];
    }
    dx.hasPostTransform = hasPost ? 1 : 0;

    dx.opsOffset = opsStart;
    dx.opsCount = static_cast<std::int32_t>(ops.size()) - opsStart;
}

DeviceXform buildXformStatic(const XForm& xform, const VariationRegistry& registry, std::vector<DeviceOp>& ops,
                              std::vector<double>& params) {
    DeviceXform dx;
    dx.c00 = xform.c[0][0];
    dx.c01 = xform.c[0][1];
    dx.c10 = xform.c[1][0];
    dx.c11 = xform.c[1][1];
    dx.c20 = xform.c[2][0];
    dx.c21 = xform.c[2][1];
    dx.colorC1 = (1 + xform.symmetry) / 2.0;
    dx.colorC2 = xform.color * (1 - xform.symmetry) / 2.0;
    dx.transOpacity = xform.transOpacity;
    dx.opacityAlwaysPasses = (dx.transOpacity == 1.0) ? 1 : 0;
    dx.pluginColor = xform.pluginColor;
    appendXformOps(xform, registry, ops, params, dx);
    return dx;
}

} // namespace

bool isFlameGpuEligible(const Flame& flame) {
    const int numXForms = flame.numXForms();
    if (numXForms <= 0 || flame.width <= 0 || flame.height <= 0) return false;

    auto& registry = VariationRegistry::instance();
    for (int i = 0; i < numXForms; ++i) {
        if (!isXFormGpuEligible(*flame.xform[i], registry)) return false;
    }
    if (flame.finalXformEnabled && flame.useFinalXform) {
        if (!isXFormGpuEligible(*flame.finalXform, registry)) return false;
    }
    return true;
}

bool buildDeviceFlame(const Flame& flame, DeviceFlameHost& out) {
    if (!isFlameGpuEligible(flame)) return false;

    const int numXForms = flame.numXForms();

    detail::RenderPlan plan;
    plan.numXForms = numXForms;
    plan.useFinalXform = flame.finalXformEnabled && flame.useFinalXform;
    plan.propTable = detail::buildPropTable(flame, numXForms);
    plan.projectionKind = detail::selectProjectionKind(flame);
    plan.cameraMatrix = detail::buildCameraMatrix(flame.cameraPitch, flame.cameraYaw);
    plan.persp = flame.cameraPersp;
    plan.zpos = flame.cameraZpos;
    plan.dofCoef = 0.1 * flame.cameraDOF;

    detail::buildFilter(flame, plan);
    detail::buildBuffersAndCamera(flame, plan);
    detail::buildColorMap(flame, plan);
    detail::buildToneMap(flame, plan);
    detail::buildCurveTables(flame, plan);
    detail::buildAdaptiveDE(flame, plan);

    out = DeviceFlameHost{};
    out.numXForms = numXForms;
    out.useFinalXform = plan.useFinalXform;
    out.propTable = std::move(plan.propTable);
    out.propTableStride = detail::kPropTableSize;

    out.projectionKind = static_cast<DeviceProjectionKind>(plan.projectionKind);
    out.cameraMatrix.m00 = plan.cameraMatrix.m00;
    out.cameraMatrix.m10 = plan.cameraMatrix.m10;
    out.cameraMatrix.m20 = plan.cameraMatrix.m20;
    out.cameraMatrix.m01 = plan.cameraMatrix.m01;
    out.cameraMatrix.m11 = plan.cameraMatrix.m11;
    out.cameraMatrix.m21 = plan.cameraMatrix.m21;
    out.cameraMatrix.m02 = plan.cameraMatrix.m02;
    out.cameraMatrix.m12 = plan.cameraMatrix.m12;
    out.cameraMatrix.m22 = plan.cameraMatrix.m22;
    out.persp = plan.persp;
    out.zpos = plan.zpos;
    out.dofCoef = plan.dofCoef;

    out.camX0 = plan.camX0;
    out.camY0 = plan.camY0;
    out.camW = plan.camW;
    out.camH = plan.camH;
    out.bws = plan.bws;
    out.bhs = plan.bhs;
    out.bucketWidth = plan.bucketWidth;
    out.bucketHeight = plan.bucketHeight;
    out.oversample = plan.oversample;
    out.maxGutterWidth = plan.maxGutterWidth;
    out.numBatches = plan.numBatches;

    out.colorMap.resize(256 * 3);
    for (int i = 0; i < 256; ++i) {
        out.colorMap[static_cast<size_t>(i) * 3 + 0] = plan.colorMap[i][0];
        out.colorMap[static_cast<size_t>(i) * 3 + 1] = plan.colorMap[i][1];
        out.colorMap[static_cast<size_t>(i) * 3 + 2] = plan.colorMap[i][2];
    }

    out.filterSize = plan.filterSize;
    out.filter = std::move(plan.filter);
    out.fastBucket = plan.fastBucket;
    out.k1 = plan.k1;
    out.k2 = plan.k2;
    out.logScale = plan.logScale;

    out.curvesSet = plan.curvesSet;
    out.curveTable = plan.curveTable;

    out.adaptiveDE = plan.adaptiveDE;
    out.deMinRadius = plan.deMinRadius;
    out.deMaxRadius = plan.deMaxRadius;
    out.deCurve = plan.deCurve;
    out.deMaxRadiusInt = plan.deMaxRadiusInt;
    // plan.deKernels is only populated when plan.adaptiveDE is true -
    // buildAdaptiveDE() returns early otherwise, leaving deMaxRadiusInt at
    // its unrelated default (1) and deKernels empty, so this must not index
    // into it unconditionally (that would be an out-of-bounds vector read).
    if (plan.adaptiveDE) {
        out.deKernelOffsets.assign(static_cast<size_t>(plan.deMaxRadiusInt) + 1, 0);
        for (int r = 1; r <= plan.deMaxRadiusInt; ++r) {
            out.deKernelOffsets[static_cast<size_t>(r)] = static_cast<int>(out.deKernelsFlat.size());
            const auto& kernel = plan.deKernels[static_cast<size_t>(r)];
            out.deKernelsFlat.insert(out.deKernelsFlat.end(), kernel.begin(), kernel.end());
        }
    }

    auto& registry = VariationRegistry::instance();
    out.xforms.reserve(static_cast<size_t>(numXForms) + (plan.useFinalXform ? 1 : 0));
    for (int i = 0; i < numXForms; ++i) {
        out.xforms.push_back(buildXformStatic(*flame.xform[i], registry, out.ops, out.params));
    }

    if (flame.soloXform >= 0 && flame.soloXform < numXForms) {
        for (int i = 0; i < numXForms; ++i) {
            out.xforms[static_cast<size_t>(i)].transOpacity = 0;
            out.xforms[static_cast<size_t>(i)].opacityAlwaysPasses = 0;
        }
        out.xforms[static_cast<size_t>(flame.soloXform)].transOpacity = 1;
        out.xforms[static_cast<size_t>(flame.soloXform)].opacityAlwaysPasses = 1;
    }

    if (plan.useFinalXform) {
        out.finalXformIndex = numXForms;
        out.xforms.push_back(buildXformStatic(*flame.finalXform, registry, out.ops, out.params));
    } else {
        out.finalXformIndex = -1;
    }

    out.width = flame.width;
    out.height = flame.height;
    out.channels = flame.transparency ? 4 : 3;
    out.gamma = flame.gamma;
    out.gammaThreshold = flame.gammaThreshold;
    out.vibrancy = flame.vibrancy;
    out.background = flame.background;
    out.transparency = flame.transparency;

    return true;
}

} // namespace apo::gpu
