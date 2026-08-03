#pragma once

#include "DeviceTypes.h"

namespace apo::gpu {

// One thread == one sub-batch (see DeviceTypes.h's comment on why that's
// the natural mapping: the CPU renderBatch() itself already redraws a fresh
// point and re-runs FUSE on every sub-batch, so sub-batches have no
// point-to-point dependency to preserve). Launches `numThreadsThisLaunch`
// threads, each handling global sub-batch index `globalBatchOffset +
// threadIdx` - GpuRenderer.cu calls this in a host-side loop, chunking
// across DeviceFlameHost::numBatches total sub-batches so progress/cancel
// can be polled between launches. `pointsGenerated`/`pointsAccepted` are
// single device `unsigned long long` counters this kernel atomicAdd's into
// (zero-initialize before the first launch of a render).
__global__ void chaosKernel(DeviceKernelParams kp, unsigned long long renderSeed, int globalBatchOffset,
                             int numThreadsThisLaunch, unsigned long long* pointsGenerated,
                             unsigned long long* pointsAccepted);

} // namespace apo::gpu
