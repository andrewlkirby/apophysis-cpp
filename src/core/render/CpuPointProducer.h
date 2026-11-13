#pragma once

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <vector>

#include "../Flame.h"
#include "PointStream.h"
#include "Renderer.h"

namespace apo::detail {

// The CPU half of a hybrid CPU+GPU render (docs/HYBRID_CPU_GPU_RENDERING_PLAN.md).
// Starts `threadCount` worker threads that run the CPU renderer's normal
// per-sub-batch chaos game (Renderer.cpp's renderBatch, with the same
// per-thread Flame clones, RNG and solo-xform handling), but instead of
// accumulating into private bucket arrays each worker writes every
// in-bounds point to `stream` as a StreamedPoint for the GPU to add.
//
// Workers claim sub-batches one at a time from `nextBatch`, the same counter
// the GPU launch loop claims from, and stop once it reaches the plan's
// numBatches (or on cancel). Progress (pointsDone) and pause/cancel use the
// same RenderProgress conventions as Renderer::render().
//
// Every chunk capacity in `stream` must be at least kSubBatchSize.
class CpuPointProducer {
public:
    CpuPointProducer(const Flame& flame, std::uint64_t seed, int threadCount, std::atomic<int>& nextBatch,
                     PointStream& stream, RenderProgress* progress);
    ~CpuPointProducer();
    CpuPointProducer(const CpuPointProducer&) = delete;
    CpuPointProducer& operator=(const CpuPointProducer&) = delete;

    // Plan geometry, for the consumer to sanity-check against its own.
    int bucketWidth() const;
    int bucketHeight() const;
    int numBatches() const;

    // True once every worker thread has exited its loop and submitted its
    // last chunk. The consumer must keep draining the stream until this is
    // true before calling join() - a worker blocked in acquireEmpty() on a
    // full pool would otherwise never finish.
    bool finished() const { return running_.load(std::memory_order_acquire) == 0; }

    // Waits for every worker to finish (after submitting its last partial
    // chunk), then rethrows the first worker exception if there was one.
    // Safe to call more than once; later calls return the same stats.
    RenderStats join();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<int> running_{0};
};

} // namespace apo::detail
