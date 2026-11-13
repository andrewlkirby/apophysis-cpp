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
__device__ void devAddToBucket(const DeviceKernelParams& kp, size_t idx, int colorIdx) {
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

    devAddToBucket(kp, static_cast<size_t>(by) * kp.bucketWidth + bx, colorIdx);
}

} // namespace

__global__ void scatterKernel(DeviceKernelParams kp, const DeviceStreamedPoint* points, int count) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    const DeviceStreamedPoint p = points[i];
    devAddToBucket(kp, p.bucket, static_cast<int>(p.color));
}

namespace {

// Fresh state for global sub-batch `globalBatchIdx`: the per-sub-batch seed
// (splitmix-mixed the same style as the CPU renderer's per-thread seeding -
// see DeviceTypes.h's mapping comment for why "per sub-batch" is this
// backend's unit of independent RNG state) and a random start point.
__device__ DeviceChaosState devInitState(unsigned long long renderSeed, long long globalBatchIdx, bool alive) {
    DeviceChaosState st;
    st.rng = DeviceRng::seeded(renderSeed + static_cast<unsigned long long>(globalBatchIdx) * 0x9e3779b97f4a7c15ULL);
    st.x = 2 * st.rng.uniform01() - 1;
    st.y = 2 * st.rng.uniform01() - 1;
    st.z = 0;
    st.c = st.rng.uniform01();
    st.xfIdx = 0;
    st.alive = alive ? 1 : 0;
    return st;
}

// splitmix64 finalizer - for the shuffle permutation, which every thread of
// a block must derive identically without communicating.
__device__ __forceinline__ unsigned long long devMix64(unsigned long long z) {
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// One accepted-or-not point at main-phase step: the opacity roll, final
// xform, projection and histogram write. Returns whether it was accepted.
__device__ __forceinline__ bool devPlotPoint(const DeviceKernelParams& kp, const DeviceXform& xf, double x, double y,
                                             double z, double c, DeviceRng& rng) {
    if (!xf.opacityAlwaysPasses && rng.uniform01() >= xf.transOpacity) return false; // opacity roll rejected
    double qx = x, qy = y, qz = z, qc = c;
    if (kp.finalXformIndex >= 0) devXformNextPoint(kp, kp.finalXformIndex, qx, qy, qz, qc, rng);
    devProject(qx, qy, qz, kp, rng);
    if (!isfinite(qx) || !isfinite(qy) || !isfinite(qz) || !isfinite(qc)) return false;
    devAccumulate(kp, qx, qy, qc);
    return true;
}

} // namespace

__global__ void chaosKernel(DeviceKernelParams kp, DeviceChaosWave wave, DeviceChaosState* states,
                             unsigned long long* pointsGenerated, unsigned long long* pointsAccepted) {
    const int slot = blockIdx.x * blockDim.x + threadIdx.x;
    if (slot >= wave.count) return;

    DeviceChaosState st = wave.stepBegin == 0
        ? devInitState(wave.renderSeed, static_cast<long long>(wave.batchOffset) + slot, true)
        : states[slot];
    if (!st.alive) return; // bailed out in an earlier launch: the rest of this sub-batch is lost

    unsigned long long localGenerated = 0;
    unsigned long long localAccepted = 0;
    for (int s = wave.stepBegin; s < wave.stepEnd; ++s) {
        st.xfIdx = kp.propTable[st.xfIdx * kp.propTableStride + st.rng.uniformInt(kp.propTableStride)];
        devXformNextPoint(kp, st.xfIdx, st.x, st.y, st.z, st.c, st.rng);
        if (s <= kDeviceFuse) continue; // FUSE warm-up: nothing plotted

        if (!isfinite(st.x) || !isfinite(st.y) || !isfinite(st.z) || !isfinite(st.c)) {
            st.alive = 0; // matches renderBatch's bailout
            break;
        }
        ++localGenerated;
        if (devPlotPoint(kp, kp.xforms[st.xfIdx], st.x, st.y, st.z, st.c, st.rng)) ++localAccepted;
    }

    if (wave.stepEnd < kDeviceSubBatchSteps) states[slot] = st;
    if (localGenerated) atomicAdd(pointsGenerated, localGenerated);
    if (localAccepted) atomicAdd(pointsAccepted, localAccepted);
}

static_assert(kChaosThreadsPerBlock == 256, "chaosKernelWarpUniform's shuffle permutes 8-bit slot indices");

__global__ void __launch_bounds__(kChaosThreadsPerBlock)
chaosKernelWarpUniform(DeviceKernelParams kp, DeviceChaosWave wave, DeviceChaosState* states,
                       unsigned long long* pointsGenerated, unsigned long long* pointsAccepted) {
    // Double-buffered so each step needs one __syncthreads: step s writes
    // buffer s&1 and reads it back after the barrier; nobody writes that
    // buffer again until step s+2, which is past step s+1's barrier.
    __shared__ double sx[2][kChaosThreadsPerBlock];
    __shared__ double sy[2][kChaosThreadsPerBlock];
    __shared__ double sz[2][kChaosThreadsPerBlock];
    __shared__ double sc[2][kChaosThreadsPerBlock];
    __shared__ unsigned char sAlive[2][kChaosThreadsPerBlock];

    const int t = threadIdx.x;
    const int lane = t & 31;
    const int slot = blockIdx.x * blockDim.x + t;

    // Slots past the end of the wave can't return early (every thread must
    // reach each barrier), so they start as dead points instead. The RNG
    // stays with the thread (slot); the points move between slots, and
    // each slot's state is saved for the next launch. GpuRenderer sizes
    // `states` to whole blocks, so every slot here has an entry.
    DeviceChaosState st = wave.stepBegin == 0
        ? devInitState(wave.renderSeed, static_cast<long long>(wave.batchOffset) + slot, slot < wave.count)
        : states[slot];
    bool alive = st.alive != 0;

    const unsigned long long blockSalt =
        devMix64(wave.renderSeed ^ (static_cast<unsigned long long>(static_cast<long long>(wave.batchOffset) +
                                                                     blockIdx.x * blockDim.x) *
                                    0xd1b54a32d192ed03ULL));

    unsigned long long localGenerated = 0;
    unsigned long long localAccepted = 0;

    for (int s = wave.stepBegin; s < wave.stepEnd; ++s) {
        // One xform for the whole warp, drawn by lane 0. Every row of the
        // prop table is identical here, so row 0 serves every point.
        int pick = 0;
        if (lane == 0) pick = st.rng.uniformInt(kp.propTableStride);
        pick = __shfl_sync(0xffffffffu, pick, 0);
        const int xfIdx = kp.propTable[pick];

        if (alive) {
            devXformNextPoint(kp, xfIdx, st.x, st.y, st.z, st.c, st.rng);
            if (s > kDeviceFuse) {
                if (!isfinite(st.x) || !isfinite(st.y) || !isfinite(st.z) || !isfinite(st.c)) {
                    alive = false; // matches chaosKernel's bailout
                } else {
                    ++localGenerated;
                    if (devPlotPoint(kp, kp.xforms[xfIdx], st.x, st.y, st.z, st.c, st.rng)) ++localAccepted;
                }
            }
        }

        // Permute the block's points: p -> rotl8((a*p + b) mod 256, r) with
        // odd a. Both steps are bijections on 0..255; the rotation moves lane
        // bits into warp bits, so points don't keep a fixed lane relation.
        // Keyed on the step index, so how a sub-batch is split across
        // launches doesn't change the result.
        const unsigned long long h = devMix64(blockSalt + static_cast<unsigned long long>(s));
        const int a = static_cast<int>(h & 0xff) | 1;
        const int b = static_cast<int>((h >> 8) & 0xff);
        const int r = static_cast<int>((h >> 16) & 7);
        int p = (a * t + b) & 0xff;
        p = ((p << r) | (p >> (8 - r))) & 0xff;
        const int buf = s & 1;
        sx[buf][p] = st.x;
        sy[buf][p] = st.y;
        sz[buf][p] = st.z;
        sc[buf][p] = st.c;
        sAlive[buf][p] = alive ? 1 : 0;
        __syncthreads();
        st.x = sx[buf][t];
        st.y = sy[buf][t];
        st.z = sz[buf][t];
        st.c = sc[buf][t];
        alive = sAlive[buf][t] != 0;
    }

    st.alive = alive ? 1 : 0;
    if (wave.stepEnd < kDeviceSubBatchSteps) states[slot] = st;
    if (localGenerated) atomicAdd(pointsGenerated, localGenerated);
    if (localAccepted) atomicAdd(pointsAccepted, localAccepted);
}

} // namespace apo::gpu
