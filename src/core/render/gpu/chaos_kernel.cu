#include "chaos_kernel.cuh"

#include <cmath>

#include "DeviceRng.cuh"
#include "DeviceVariations.cuh"

namespace apo::gpu {

namespace {

// 3D -> 2D camera projection - device port of Renderer.cpp's project<RngT>()
// template, formula-for-formula identical (see that function's own doc
// comment for the DOF branches' provenance).
__device__ void devProject(double& x, double& y, double& z, const DeviceKernelParams& kp, DeviceRng& rng) {
    const double origZ = z;
    const DeviceCameraMatrix& m = kp.cameraMatrix;
    switch (kp.projectionKind) {
        case DeviceProjectionKind::None: {
            const double zr = 1 - kp.persp * (z - kp.zpos);
            x /= zr;
            y /= zr;
            z = origZ - kp.zpos;
            break;
        }
        case DeviceProjectionKind::Pitch: {
            const double zPrime = z - kp.zpos;
            const double yRot = m.m11 * y + m.m21 * zPrime;
            const double zRot = m.m12 * y + m.m22 * zPrime;
            const double zr = 1 - kp.persp * zRot;
            x = x / zr;
            y = yRot / zr;
            z = origZ - kp.zpos;
            break;
        }
        case DeviceProjectionKind::PitchYaw: {
            const double zPrime = z - kp.zpos;
            const double xRot = m.m00 * x + m.m10 * y;
            const double yRot = m.m01 * x + m.m11 * y + m.m21 * zPrime;
            const double zRot = m.m02 * x + m.m12 * y + m.m22 * zPrime;
            const double zr = 1 - kp.persp * zRot;
            x = xRot / zr;
            y = yRot / zr;
            z = origZ - kp.zpos;
            break;
        }
        case DeviceProjectionKind::PitchDOF: {
            const double zPrime = z - kp.zpos;
            const double yRot = m.m11 * y + m.m21 * zPrime;
            const double zRot = m.m12 * y + m.m22 * zPrime;
            const double zr = 1 - kp.persp * zRot;
            const double t = rng.uniform01() * kDeviceTwoPi;
            const double dsin = sin(t), dcos = cos(t);
            const double dr = rng.uniform01() * kp.dofCoef * zRot;
            x = (x + dr * dcos) / zr;
            y = (yRot + dr * dsin) / zr;
            z = origZ - kp.zpos;
            break;
        }
        case DeviceProjectionKind::PitchYawDOF: {
            const double zPrime = z - kp.zpos;
            const double xRot = m.m00 * x + m.m10 * y;
            const double yRot = m.m01 * x + m.m11 * y + m.m21 * zPrime;
            const double zRot = m.m02 * x + m.m12 * y + m.m22 * zPrime;
            const double zr = 1 - kp.persp * zRot;
            const double t = rng.uniform01() * kDeviceTwoPi;
            const double dsin = sin(t), dcos = cos(t);
            const double dr = rng.uniform01() * kp.dofCoef * zRot;
            x = (xRot + dr * dcos) / zr;
            y = (yRot + dr * dsin) / zr;
            z = origZ - kp.zpos;
            break;
        }
    }
}

// Device port of Renderer.cpp's accumulate<Scalar>() - same bucket-index
// math, but atomicAdd instead of a plain `+=` since every GPU thread writes
// into the same shared histogram (no per-thread private bucket array on
// this backend - see DeviceTypes.h's mapping comment). useFloatBuckets is a
// per-render constant (never varies point-to-point), so this branch is
// uniform across every thread in the kernel - no warp divergence cost,
// just picks which atomicAdd overload/array gets used for the whole launch.
__device__ void devAccumulate(const DeviceKernelParams& kp, double x, double y, double colorCoord) {
    const double px = x - kp.camX0;
    if (px < 0 || px > kp.camW) return;
    const double py = y - kp.camY0;
    if (py < 0 || py > kp.camH) return;

    int bx = static_cast<int>(llround(kp.bws * px));
    int by = static_cast<int>(llround(kp.bhs * py));
    bx = min(max(bx, 0), kp.bucketWidth - 1);
    by = min(max(by, 0), kp.bucketHeight - 1);

    int colorIdx = static_cast<int>(llround(colorCoord * 255));
    colorIdx = min(max(colorIdx, 0), 255);

    const size_t idx = static_cast<size_t>(by) * kp.bucketWidth + bx;
    if (kp.useFloatBuckets) {
        DeviceBucketF& b = kp.bucketsF[idx];
        atomicAdd(&b.red, static_cast<float>(kp.colorMap[colorIdx * 3 + 0]));
        atomicAdd(&b.green, static_cast<float>(kp.colorMap[colorIdx * 3 + 1]));
        atomicAdd(&b.blue, static_cast<float>(kp.colorMap[colorIdx * 3 + 2]));
        atomicAdd(&b.count, 1.0f);
    } else {
        DeviceBucket& b = kp.bucketsD[idx];
        atomicAdd(&b.red, kp.colorMap[colorIdx * 3 + 0]);
        atomicAdd(&b.green, kp.colorMap[colorIdx * 3 + 1]);
        atomicAdd(&b.blue, kp.colorMap[colorIdx * 3 + 2]);
        atomicAdd(&b.count, 1.0);
    }
}

} // namespace

__global__ void chaosKernel(DeviceKernelParams kp, unsigned long long renderSeed, int globalBatchOffset,
                             int numThreadsThisLaunch, unsigned long long* pointsGenerated,
                             unsigned long long* pointsAccepted) {
    const int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= numThreadsThisLaunch) return;
    const int globalBatchIdx = globalBatchOffset + tid;

    // Distinct per-sub-batch seed, splitmix-mixed the same style as the CPU
    // renderer's per-thread seeding (Renderer.cpp's renderImpl) - see
    // DeviceTypes.h's mapping comment for why "per sub-batch" (not "per CPU
    // worker thread") is this backend's unit of independent RNG state.
    DeviceRng rng = DeviceRng::seeded(renderSeed + static_cast<unsigned long long>(globalBatchIdx) *
                                                        0x9e3779b97f4a7c15ULL);

    double x = 2 * rng.uniform01() - 1;
    double y = 2 * rng.uniform01() - 1;
    double z = 0;
    double c = rng.uniform01();
    int xfIdx = 0;

    for (int i = 0; i <= kDeviceFuse; ++i) {
        xfIdx = kp.propTable[xfIdx * kp.propTableStride + rng.uniformInt(kp.propTableStride)];
        devXformNextPoint(kp, xfIdx, x, y, z, c, rng);
    }

    unsigned long long localGenerated = 0;
    unsigned long long localAccepted = 0;

    for (int i = 0; i < kDeviceSubBatchSize; ++i) {
        xfIdx = kp.propTable[xfIdx * kp.propTableStride + rng.uniformInt(kp.propTableStride)];
        devXformNextPoint(kp, xfIdx, x, y, z, c, rng);

        if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(c)) break; // matches renderBatch's bailout

        ++localGenerated;

        const DeviceXform& xf = kp.xforms[xfIdx];
        if (!xf.opacityAlwaysPasses && rng.uniform01() >= xf.transOpacity) continue; // opacity roll rejected

        double qx = x, qy = y, qz = z, qc = c;
        if (kp.finalXformIndex >= 0) devXformNextPoint(kp, kp.finalXformIndex, qx, qy, qz, qc, rng);
        devProject(qx, qy, qz, kp, rng);

        if (!isfinite(qx) || !isfinite(qy) || !isfinite(qz) || !isfinite(qc)) continue;

        devAccumulate(kp, qx, qy, qc);
        ++localAccepted;
    }

    atomicAdd(pointsGenerated, localGenerated);
    atomicAdd(pointsAccepted, localAccepted);
}

} // namespace apo::gpu
