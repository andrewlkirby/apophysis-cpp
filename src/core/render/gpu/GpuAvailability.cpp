#include "GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include <cuda_runtime_api.h>
#endif

namespace apo::gpu {

#ifdef APO_HAVE_CUDA

namespace {
// cudaGetDeviceCount/cudaGetDeviceProperties are host-side CUDA Runtime API
// calls - declared in cuda_runtime_api.h and implemented in cudart - so this
// file compiles as plain C++ via the normal host compiler (no nvcc needed
// for this translation unit) as long as it links against CUDA::cudart. Only
// the actual kernels (chaos_kernel.cu/tonemap_kernel.cu) need nvcc.
int queryDeviceCount(cudaError_t* outStatus) {
    int count = 0;
    cudaError_t status = cudaGetDeviceCount(&count);
    if (outStatus) *outStatus = status;
    if (status != cudaSuccess) return 0;
    return count;
}
} // namespace

bool isCudaAvailable() { return cudaDeviceCount() > 0; }

std::string cudaUnavailableReason() {
    cudaError_t status = cudaSuccess;
    const int count = queryDeviceCount(&status);
    if (status != cudaSuccess) {
        return std::string("CUDA runtime error: ") + cudaGetErrorString(status);
    }
    if (count == 0) {
        return "No CUDA-capable GPU detected";
    }
    return {};
}

int cudaDeviceCount() { return queryDeviceCount(nullptr); }

#else // !APO_HAVE_CUDA

bool isCudaAvailable() { return false; }

std::string cudaUnavailableReason() {
    return "Built without CUDA support (APO_ENABLE_CUDA was OFF)";
}

int cudaDeviceCount() { return 0; }

#endif

} // namespace apo::gpu
