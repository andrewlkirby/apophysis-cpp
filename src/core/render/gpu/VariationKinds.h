#pragma once

#include <string>
#include <vector>

// Plain C++ (no CUDA syntax) so this is includable from both host-only
// translation units (RenderDispatcher.cpp, DeviceFlameBuilder.cpp) and
// device .cu files - it is the single source of truth for "which
// variations does the GPU backend actually implement", used both to decide
// whether a flame is GPU-eligible (RenderDispatcher) and to build each
// xform's device op list (DeviceFlameBuilder).

namespace apo::gpu {

// Local variations (0..28) always match VariationRegistry::kNumLocalVars's
// own fixed index space one-to-one (see VariationRegistry.cpp's
// localVarNames()) - every one of the 29 is ported (DeviceVariations.cuh),
// so no name lookup is needed for them; a local variation's device op kind
// is just its host index, unmapped.
constexpr int kNumLocalVars = 29;

// Registered (non-local) variations ported to the GPU. Only names appearing
// in kRegisteredVarTable (VariationKinds.cpp) render on the GPU; anything
// else weighted on a flame - an unported native variation, or any of the 47
// legacy C plugins, which are never added to this table - makes
// RenderDispatcher fall the whole flame back to the CPU renderer. Kept as a
// plain int (not an enum) so DeviceVariations.cuh's dispatch switch and this
// table can't drift apart from a forgotten enumerator; see
// VariationKinds.cpp's kRegisteredVarTable for the authoritative list and
// DeviceVariations.cuh's switch for where each id's math lives.
constexpr int kFirstRegisteredKind = 1000;

// Kind ids for every GPU-ported registered variation - DeviceVariations.cuh's
// dispatch switch has one `case kind::kXxx:` per entry, and VariationKinds.cpp's
// table (the host-side name lookup) maps each variation's string name to the
// same constant, so the two can never drift out of sync silently (a
// forgotten case would fail to compile as "unhandled", not silently mis-map
// at runtime, once DeviceVariations.cuh's switch's default case asserts).
namespace kind {
constexpr int kAuger = kFirstRegisteredKind + 0;
constexpr int kBent = kFirstRegisteredKind + 1;
constexpr int kBlob = kFirstRegisteredKind + 2;
constexpr int kCosine = kFirstRegisteredKind + 3;
constexpr int kCross = kFirstRegisteredKind + 4;
constexpr int kExponential = kFirstRegisteredKind + 5;
constexpr int kFoci = kFirstRegisteredKind + 6;
constexpr int kHeart = kFirstRegisteredKind + 7;
constexpr int kHemisphere = kFirstRegisteredKind + 8;
constexpr int kLazysusan = kFirstRegisteredKind + 9;
constexpr int kMobius = kFirstRegisteredKind + 10;
constexpr int kPdj = kFirstRegisteredKind + 11;
constexpr int kPopcorn = kFirstRegisteredKind + 12;
constexpr int kPower = kFirstRegisteredKind + 13;
constexpr int kPreSinusoidal = kFirstRegisteredKind + 14;
constexpr int kPreSpherical = kFirstRegisteredKind + 15;
constexpr int kSecant2 = kFirstRegisteredKind + 16;
constexpr int kSeparation = kFirstRegisteredKind + 17;
constexpr int kSplits = kFirstRegisteredKind + 18;
constexpr int kTangent = kFirstRegisteredKind + 19;
constexpr int kWaves2 = kFirstRegisteredKind + 20;
// Batch 2 - "prepare()-only" variations (no RNG, no selectCalcFunction
// specialization): their prepare()-derived constants are pure functions of
// the raw named parameters, so the device calc() functions below just
// recompute them inline each call rather than needing a separate host-side
// precompute step - see DeviceVariations.cuh's comment on this batch.
constexpr int kBipolar = kFirstRegisteredKind + 21;
constexpr int kBwraps = kFirstRegisteredKind + 22;
constexpr int kCurl3D = kFirstRegisteredKind + 23;
constexpr int kElliptic = kFirstRegisteredKind + 24;
constexpr int kEscher = kFirstRegisteredKind + 25;
constexpr int kFan = kFirstRegisteredKind + 26;
constexpr int kFan2 = kFirstRegisteredKind + 27;
constexpr int kLog = kFirstRegisteredKind + 28;
constexpr int kLoonie = kFirstRegisteredKind + 29;
constexpr int kNGon = kFirstRegisteredKind + 30;
constexpr int kPerspective = kFirstRegisteredKind + 31;
constexpr int kPolar2 = kFirstRegisteredKind + 32;
constexpr int kPostBwraps = kFirstRegisteredKind + 33;
constexpr int kPostCurl = kFirstRegisteredKind + 34;
constexpr int kPostCurl3D = kFirstRegisteredKind + 35;
constexpr int kPreBwraps = kFirstRegisteredKind + 36;
constexpr int kPreDisc = kFirstRegisteredKind + 37;
constexpr int kRings = kFirstRegisteredKind + 38;
constexpr int kRings2 = kFirstRegisteredKind + 39;
constexpr int kScry = kFirstRegisteredKind + 40;
constexpr int kWaves = kFirstRegisteredKind + 41;
constexpr int kWedge = kFirstRegisteredKind + 42;
// Batch 3 - RNG-using and/or selectCalcFunction()-specialized variations.
// Most specializations here (Curl, Julian/JuliaScope/Julia3Dz/Julia3Djf,
// RadialBlur) are verified-algebraically-equivalent fast paths for special
// parameter values (see each device function's own comment) - the device
// side always uses the one general formula and skips the specialization
// entirely, exactly as safe as it is on the CPU side to always call calc()
// instead of the specialized branch. Rectangles' specialization is a real
// divide-by-zero guard, replicated on device. Falloff2/PostFalloff2/
// PreFalloff2's 3-way blurtype dispatch is a genuine behavioral branch
// (three different formulas), also replicated on device.
constexpr int kArch = kFirstRegisteredKind + 43;
constexpr int kBlade = kFirstRegisteredKind + 44;
constexpr int kBlurCircle = kFirstRegisteredKind + 45;
constexpr int kBlurPixelize = kFirstRegisteredKind + 46;
constexpr int kBlurZoom = kFirstRegisteredKind + 47;
constexpr int kCrop = kFirstRegisteredKind + 48;
constexpr int kPostCrop = kFirstRegisteredKind + 49;
constexpr int kPreCrop = kFirstRegisteredKind + 50;
constexpr int kCurl = kFirstRegisteredKind + 51;
constexpr int kRectangles = kFirstRegisteredKind + 52;
constexpr int kEpispiral = kFirstRegisteredKind + 53;
constexpr int kPie = kFirstRegisteredKind + 54;
constexpr int kRays = kFirstRegisteredKind + 55;
constexpr int kTwintrian = kFirstRegisteredKind + 56;
constexpr int kJulian = kFirstRegisteredKind + 57;
constexpr int kJuliaScope = kFirstRegisteredKind + 58;
constexpr int kJulia3Dz = kFirstRegisteredKind + 59;
constexpr int kJulia3Djf = kFirstRegisteredKind + 60;
constexpr int kRadialBlur = kFirstRegisteredKind + 61;
constexpr int kFalloff2 = kFirstRegisteredKind + 62;
constexpr int kPostFalloff2 = kFirstRegisteredKind + 63;
constexpr int kPreFalloff2 = kFirstRegisteredKind + 64;
} // namespace kind

struct RegisteredVarInfo {
    int kind = -1;
    // Named-parameter list in the exact order DeviceFlameBuilder must pack
    // them into a DeviceFlame param block, and the exact order
    // DeviceVariations.cuh's device calc function reads ctx.params[0..].
    // Matches each ported variation's own variableNameAt() order.
    std::vector<std::string> paramNames;
};

// Returns the device kind + param-name list for a registered (non-local)
// variation name, or a default-constructed (kind == -1) result if this
// variation has no GPU implementation.
const RegisteredVarInfo* lookupRegisteredVarKind(const std::string& name);

} // namespace apo::gpu
