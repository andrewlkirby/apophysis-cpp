#pragma once

#include <string>

namespace apo::gpu {

// Runtime CUDA device query - compiled unconditionally (regardless of
// APO_ENABLE_CUDA), so callers (RenderDispatcher, the UI's "Use GPU"
// preference toggle) can always ask "is a GPU render path available right
// now?" without needing an #ifdef of their own. When this project wasn't
// built with APO_ENABLE_CUDA at all, isAvailable() always returns false and
// unavailableReason() explains why - the same shape a build that WAS
// compiled with CUDA support returns when the toolkit is present but no
// capable device is found at runtime (no NVIDIA GPU, driver too old, etc).
// See docs/GPU_RENDERING_PLAN.md's "Build integration" section.
bool isCudaAvailable();

// Human-readable explanation of the current isCudaAvailable() result -
// "" when available, otherwise why not (no CUDA build, no device, driver
// too old, etc). Meant for a UI status line / log message, not parsing.
std::string cudaUnavailableReason();

// Convenience: 0 if unavailable, else the number of CUDA-capable devices
// visible to this process. Never throws.
int cudaDeviceCount();

} // namespace apo::gpu
