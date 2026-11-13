#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace apo::detail {

// Hybrid CPU+GPU rendering (docs/HYBRID_CPU_GPU_RENDERING_PLAN.md): CPU
// worker threads don't keep their own histograms in hybrid mode. Each point
// that would have been accumulated is written out as one of these instead,
// and the GPU side adds it into the shared device histogram with the same
// math the chaos kernel uses. `bucket` is the flat bucket index
// (by * bucketWidth + bx), `color` the 0..255 color-map index.
struct StreamedPoint {
    std::uint32_t bucket;
    std::uint32_t color;
};

// A fixed pool of point chunks shared between CPU producer threads and one
// consumer (the GPU driver thread). The storage behind each chunk is owned
// by the caller (pinned host memory on the GPU side) and outlives the
// stream. Producers acquireEmpty() -> fill -> submitFull(); the consumer
// tryPopFull() -> upload -> release(). acquireEmpty() blocks while every
// chunk is in use, which is the backpressure that stops the CPU from running
// arbitrarily far ahead of the GPU.
class PointStream {
public:
    struct Chunk {
        StreamedPoint* data = nullptr;
        int capacity = 0;
        int count = 0;
    };

    explicit PointStream(std::vector<Chunk> chunks) : chunks_(std::move(chunks)) {
        for (Chunk& c : chunks_) empty_.push_back(&c);
    }
    PointStream(const PointStream&) = delete;
    PointStream& operator=(const PointStream&) = delete;

    // Blocks until a chunk is free. Returns nullptr once abort() was called.
    Chunk* acquireEmpty() {
        std::unique_lock<std::mutex> lock(mutex_);
        emptyAvailable_.wait(lock, [&] { return aborted_ || !empty_.empty(); });
        if (aborted_) return nullptr;
        Chunk* c = empty_.front();
        empty_.pop_front();
        c->count = 0;
        return c;
    }

    // Hands a filled (possibly partially filled) chunk to the consumer.
    // An empty chunk goes straight back to the free list.
    void submitFull(Chunk* c) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (c->count == 0) {
            empty_.push_back(c);
            emptyAvailable_.notify_one();
        } else {
            full_.push_back(c);
        }
    }

    // Consumer side, non-blocking.
    Chunk* tryPopFull() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (full_.empty()) return nullptr;
        Chunk* c = full_.front();
        full_.pop_front();
        return c;
    }

    // Consumer side: whether any submitted chunk is waiting.
    bool hasFull() {
        std::lock_guard<std::mutex> lock(mutex_);
        return !full_.empty();
    }

    // Consumer side: the chunk's contents have been consumed (uploaded).
    void release(Chunk* c) {
        std::lock_guard<std::mutex> lock(mutex_);
        c->count = 0;
        empty_.push_back(c);
        emptyAvailable_.notify_one();
    }

    // Wakes every blocked producer; all later acquireEmpty() calls return
    // nullptr. Used when the GPU side fails mid-render.
    void abort() {
        std::lock_guard<std::mutex> lock(mutex_);
        aborted_ = true;
        abortedFlag_.store(true, std::memory_order_relaxed);
        emptyAvailable_.notify_all();
    }

    // Lock-free check producers make once per sub-batch, so an aborted
    // render stops within one sub-batch rather than at the next chunk.
    bool isAborted() const { return abortedFlag_.load(std::memory_order_relaxed); }

private:
    std::vector<Chunk> chunks_;
    std::mutex mutex_;
    std::condition_variable emptyAvailable_;
    std::deque<Chunk*> empty_;
    std::deque<Chunk*> full_;
    bool aborted_ = false;
    std::atomic<bool> abortedFlag_{false};
};

} // namespace apo::detail
