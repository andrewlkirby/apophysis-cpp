#pragma once

#include "DeviceRng.cuh"
#include "DeviceTypes.h"

namespace apo::gpu {

// Steps in one sub-batch: kDeviceFuse + 1 warm-up steps (nothing plotted),
// then kDeviceSubBatchSize plotted points - the same counts as the CPU
// renderer's renderBatch.
constexpr int kDeviceSubBatchSteps = kDeviceFuse + 1 + kDeviceSubBatchSize;

// One chaos-game trajectory's state, kept in device memory between kernel
// launches so a sub-batch can span several launches (see DeviceChaosWave).
struct DeviceChaosState {
    DeviceRng rng;
    double x, y, z, c;
    int xfIdx; // previous xform (chaosKernel; xaos picks the next from its row)
    int alive; // 0 once the point went non-finite - the rest of the sub-batch is lost
};

// What one chaos-kernel launch runs: steps [stepBegin, stepEnd) of the
// sub-batches batchOffset .. batchOffset + count - 1, one per slot (thread).
//
// Why sub-batches span launches: launches have to stay short (~0.35s, well
// under Windows' 2s GPU watchdog), and a sub-batch is 10,016 steps. When a
// whole sub-batch ran inside one launch, an expensive flame's per-thread
// latency alone filled the time budget, so launches shrank to ~1000
// threads and left most of the GPU idle (measured 22-35M pts/s steady vs
// ~70M for a 4096-thread launch on a real 5-xform flame). Now a fixed pool
// of slots sized to fill the GPU (a "wave") advances every sub-batch by
// however many steps fit the budget, and the step count adapts instead of
// the thread count.
//
// stepBegin == 0 starts the wave's sub-batches (fresh seed and start point
// per sub-batch, same seeding as before); otherwise each slot resumes from
// states[slot]. State is saved unless stepEnd == kDeviceSubBatchSteps.
// Each sub-batch runs exactly the same RNG draws however the wave is
// split into launches.
struct DeviceChaosWave {
    unsigned long long renderSeed = 0;
    int batchOffset = 0;
    int count = 0;
    int stepBegin = 0;
    int stepEnd = 0;
};

// Per-thread xform choice: every slot draws its own xform each step (with
// xaos via each point's previous xform). `states` holds at least `count`
// entries. `pointsGenerated`/`pointsAccepted` are single device counters
// this kernel atomicAdd's into (zero them before a render's first launch).
__global__ void chaosKernel(DeviceKernelParams kp, DeviceChaosWave wave, DeviceChaosState* states,
                             unsigned long long* pointsGenerated, unsigned long long* pointsAccepted);

// Threads per block for both chaos kernels. chaosKernelWarpUniform's
// shared-memory shuffle is written for exactly this size.
constexpr int kChaosThreadsPerBlock = 256;

// Same contract as chaosKernel (same wave/state handling, same sub-batch
// accounting, same points-generated/accepted totals), for flames whose xforms run
// different variation lists. chaosKernel lets every thread pick its own
// xform, so a warp whose 32 threads picked several different xforms runs
// each of their code paths in turn (warp divergence) - measured ~8x slower
// than the per-xform rates on a real 5-xform flame. Here all 32 threads of
// a warp apply the same randomly chosen xform each step (no divergence),
// and after every step the block's 256 points are permuted across its 8
// warps through shared memory, so points don't keep sharing an xform
// sequence (which, for contractive maps, would pull them onto the same
// trajectory). Each point still sees xforms drawn independently with the
// right weights, so the image converges to the same result. A point that
// goes non-finite is marked dead and carried through the shuffle without
// producing more points - the same "rest of this sub-batch is lost" rule
// as chaosKernel's `break`. Requires every row of kp.propTable to be the
// same (no xaos), since the warp's choice can't depend on each point's
// previous xform; GpuRenderer checks that before using it.
// `states` must hold every slot of every launched block (count rounded up
// to kChaosThreadsPerBlock), since dead tail slots take part in the shuffle.
__global__ void chaosKernelWarpUniform(DeviceKernelParams kp, DeviceChaosWave wave, DeviceChaosState* states,
                                       unsigned long long* pointsGenerated, unsigned long long* pointsAccepted);

// Hybrid CPU+GPU rendering: adds `count` CPU-produced points into the same
// device histogram chaosKernel accumulates into (kp.bucketsD/bucketsF,
// kp.colorMap), one thread per point, with chaosKernel's own atomicAdd math.
__global__ void scatterKernel(DeviceKernelParams kp, const DeviceStreamedPoint* points, int count);

} // namespace apo::gpu
