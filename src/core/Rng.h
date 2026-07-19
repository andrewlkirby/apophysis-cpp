#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace apo {

// Not <cmath>'s M_PI: that macro is non-standard and MSVC only defines it
// behind _USE_MATH_DEFINES, which would have to be set before every
// transitive include of <cmath> in any translation unit that pulls this
// header in - simpler to just spell out the constant.
inline constexpr double kTwoPi = 6.283185307179586476925286766559;

// Abstract RNG source used by XForm's stochastic variations (noise, blur,
// gaussian_blur, zblur, blur3D, pre_blur - see XForm::noise() etc.).
//
// The original Delphi code called the RTL's global Random()/Randomize()
// directly from inside these variation procedures - a global, mutable,
// unsynchronized stream shared by every render thread's TXForm. The Delphi
// side's own optimization notes (OPTIMIZATION_STATUS.md, Fix #1) already
// identify this as a real race in ControlPoint.pas's IterateXYC and fixed it
// there with a per-TControlPoint-instance RNG; the equivalent race inside
// XForm's own gauss_rnd-based variations was flagged but deliberately left
// unfixed in Delphi (would have required changing the shared variation
// dispatch signature used by 100+ variation files).
//
// This interface exists so the C++ port never reintroduces that race: every
// XForm is bound to an Rng reference (normally one per render thread, per
// Phase 3's threading plan) instead of touching a global. Two engines are
// provided for now: MtRng (std::mt19937_64, good enough for Phase 1
// correctness testing) and Phase 3 will add a faster PCG32/xoshiro256**
// implementation behind the same interface without touching XForm.
class Rng {
public:
    virtual ~Rng() = default;

    // Uniform double in [0, 1), matching Delphi's `random` (no-arg form).
    virtual double uniform01() = 0;

    // Uniform angle in [0, 2*pi), a convenience used by several variations.
    double uniformAngle() { return uniform01() * kTwoPi; }

    // Uniform integer in [0, exclusiveUpper), matching Delphi's `Random(N)`
    // - implemented as multiply-and-truncate from uniform01() (Delphi's own
    // Random(N) is exactly this: a truncated scale of its internal float
    // stream, not a proper unbiased partition), so variations ported from
    // Pascal's `random(N)` calls (e.g. julian's absN xform-branch pick) stay
    // statistically equivalent rather than switching to a different integer
    // RNG algorithm mid-port.
    int uniformInt(int exclusiveUpper) { return static_cast<int>(uniform01() * exclusiveUpper); }
};

class MtRng final : public Rng {
public:
    explicit MtRng(uint64_t seed) : engine_(seed) {}

    double uniform01() override { return dist_(engine_); }

private:
    std::mt19937_64 engine_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
};

// xoshiro256** 1.0 (Blackman/Vigna, public domain) - Phase 3's per-render-
// thread RNG, replacing Delphi's per-instance x86-asm LCG (AsmRandom.pas's
// FLocalRandSeed / Fix #1 in OPTIMIZATION_STATUS.md). The plan explicitly
// only requires "statistically equivalent-looking output", not a bit-exact
// stream match - multithreaded Delphi renders already weren't
// bit-reproducible across threads even in the original (Fix #1's accepted
// tradeoff), so there was never an exact stream to preserve here. xoshiro256**
// passes BigCrush/PractRand, has a 2^256-1 period, and its `next()` is a
// handful of shifts/xors - faster than mt19937_64 and much faster than an
// x86 LCG's single 32-bit multiply-add would suggest once you account for
// the LCG's much weaker statistical quality at this scale (PROP_TABLE_SIZE
// draws, per-point coordinate draws, DOF blur draws - millions per second).
class Xoshiro256Rng final : public Rng {
public:
    // seed defaults to 0 so ThreadState (Renderer.cpp, FOLLOWUP_PLAN.txt
    // B2a) can hold one by value and default-construct a vector of them
    // before assigning each its real per-thread seed, instead of needing a
    // heap-allocated unique_ptr<Rng> just to defer construction - the
    // default-seeded state is never actually drawn from before being
    // overwritten.
    explicit Xoshiro256Rng(uint64_t seed = 0) {
        // xoshiro256** state must never be all-zero; seed all 4 words via
        // splitmix64 (Vigna's recommended seeding scheme) so a single
        // integer seed - not a full 256-bit one - still fills the state
        // well, with no correlation between adjacent seeds' initial output.
        uint64_t z = seed;
        for (auto& s : state_) {
            z += 0x9e3779b97f4a7c15ULL;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            s = x ^ (x >> 31);
        }
    }

    double uniform01() override {
        // Top 53 bits of a 64-bit draw -> uniform double in [0, 1), the
        // standard construction (matches how std::uniform_real_distribution
        // itself extracts bits from a 64-bit generator).
        return static_cast<double>(next() >> 11) * 0x1.0p-53;
    }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    uint64_t next() {
        const uint64_t result = rotl(state_[1] * 5, 7) * 9;
        const uint64_t t = state_[1] << 17;

        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = rotl(state_[3], 45);

        return result;
    }

    std::array<uint64_t, 4> state_;
};

// One-time randomized-default helper for Variation constructors (e.g. pdj's
// coefficients default to `6*Random-3`, julian's power defaults to
// `Random(5)+2`). This is a *different* concern from the per-render-thread
// Rng above: it fires once, when a variation is newly instantiated (matching
// Delphi's TVariationXxx.Create calling the RTL's global Random at that
// moment), not per rendered point - so it's not part of the render hot path
// and not subject to the same cross-thread race the per-thread Rng exists to
// avoid. A single thread_local generator is enough: construction happens on
// whichever thread is building the flame (typically the UI thread), never
// concurrently with itself.
inline double constructionRandom01() {
    thread_local std::mt19937_64 engine{std::random_device{}()};
    thread_local std::uniform_real_distribution<double> dist{0.0, 1.0};
    return dist(engine);
}

} // namespace apo
