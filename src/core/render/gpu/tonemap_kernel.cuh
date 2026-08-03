#pragma once

#include "DeviceTypes.h"

namespace apo::gpu {

// One thread per output pixel (grid-stride not needed - launched with
// exactly width*height threads, see GpuRenderer.cu). Port of Renderer.cpp's
// CPU toneMap() fixed-filter path plus its adaptive-DE refinement - see
// tonemap_kernel.cu for the per-line mapping.
__global__ void tonemapKernel(DeviceToneMapParams tp);

} // namespace apo::gpu
