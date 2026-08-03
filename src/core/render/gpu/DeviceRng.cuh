#pragma once

#include <cstdint>

namespace apo::gpu {

// Device-side port of Rng.h's Xoshiro256Rng - verbatim algorithm (xoshiro256**
// 1.0, Blackman/Vigna, public domain), just __device__-qualified and with no
// virtual dispatch (CUDA device code has no use for the CPU-side Rng
// interface's polymorphism - one DeviceRng lives in each GPU thread's local
// state, never shared, so there's nothing to abstract over). See Rng.h's own
// comment for why xoshiro256** was chosen over the original's asm LCG.
struct DeviceRng {
    std::uint64_t state[4];

    __device__ static DeviceRng seeded(std::uint64_t seed) {
        DeviceRng r{};
        std::uint64_t z = seed;
        for (int i = 0; i < 4; ++i) {
            z += 0x9e3779b97f4a7c15ULL;
            std::uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            r.state[i] = x ^ (x >> 31);
        }
        return r;
    }

    __device__ static std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    __device__ std::uint64_t next() {
        const std::uint64_t result = rotl(state[1] * 5, 7) * 9;
        const std::uint64_t t = state[1] << 17;

        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = rotl(state[3], 45);

        return result;
    }

    __device__ double uniform01() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }

    __device__ double uniformAngle();

    __device__ int uniformInt(int exclusiveUpper) { return static_cast<int>(uniform01() * exclusiveUpper); }
};

// kTwoPi matches Rng.h's own constant exactly (needed by uniformAngle here
// and by chaos_kernel.cu's DOF projection branches).
inline constexpr double kDeviceTwoPi = 6.283185307179586476925286766559;

__device__ inline double DeviceRng::uniformAngle() { return uniform01() * kDeviceTwoPi; }

} // namespace apo::gpu
