#include "VariationKinds.h"

#include <unordered_map>

#include "../../VariationRegistry.h"

namespace apo::gpu {

namespace {

// Name -> {device kind, ordered param names}. Names and param-name spelling
// match each variation's own kName/variableNameAt() exactly (see
// src/core/variations/Var*.cpp) - these are also the .flame XML attribute
// names, so a mismatch here wouldn't just misroute a lookup, it would
// silently read the wrong flame parameter into the wrong device slot.
const std::unordered_map<std::string, RegisteredVarInfo>& registeredVarTable() {
    static const std::unordered_map<std::string, RegisteredVarInfo> table = {
        {"auger", {kind::kAuger, {"auger_freq", "auger_weight", "auger_scale", "auger_sym"}}},
        {"bent", {kind::kBent, {}}},
        {"blob", {kind::kBlob, {"blob_low", "blob_high", "blob_waves"}}},
        {"cosine", {kind::kCosine, {}}},
        {"cross", {kind::kCross, {}}},
        {"exponential", {kind::kExponential, {}}},
        {"foci", {kind::kFoci, {}}},
        {"heart", {kind::kHeart, {}}},
        {"hemisphere", {kind::kHemisphere, {}}},
        {"lazysusan", {kind::kLazysusan, {"lazysusan_spin", "lazysusan_space", "lazysusan_twist", "lazysusan_x",
                                           "lazysusan_y"}}},
        {"mobius", {kind::kMobius, {"Re_A", "Im_A", "Re_B", "Im_B", "Re_C", "Im_C", "Re_D", "Im_D"}}},
        {"pdj", {kind::kPdj, {"pdj_a", "pdj_b", "pdj_c", "pdj_d"}}},
        {"popcorn", {kind::kPopcorn, {}}},
        {"power", {kind::kPower, {}}},
        {"pre_sinusoidal", {kind::kPreSinusoidal, {}}},
        {"pre_spherical", {kind::kPreSpherical, {}}},
        {"secant2", {kind::kSecant2, {}}},
        {"separation", {kind::kSeparation, {"separation_x", "separation_y", "separation_xinside",
                                             "separation_yinside"}}},
        {"splits", {kind::kSplits, {"splits_x", "splits_y"}}},
        {"tangent", {kind::kTangent, {}}},
        {"waves2", {kind::kWaves2, {"waves2_freqx", "waves2_freqy", "waves2_freqz", "waves2_scalex",
                                     "waves2_scaley", "waves2_scalez"}}},
        {"bipolar", {kind::kBipolar, {"bipolar_shift"}}},
        {"bwraps", {kind::kBwraps, {"bwraps_cellsize", "bwraps_space", "bwraps_gain", "bwraps_inner_twist",
                                     "bwraps_outer_twist"}}},
        {"curl3D", {kind::kCurl3D, {"curl3D_cx", "curl3D_cy", "curl3D_cz"}}},
        {"elliptic", {kind::kElliptic, {}}},
        {"escher", {kind::kEscher, {"escher_beta"}}},
        {"fan", {kind::kFan, {}}},
        {"fan2", {kind::kFan2, {"fan2_x", "fan2_y"}}},
        {"log", {kind::kLog, {"log_base"}}},
        {"loonie", {kind::kLoonie, {}}},
        {"ngon", {kind::kNGon, {"ngon_sides", "ngon_power", "ngon_circle", "ngon_corners"}}},
        {"perspective", {kind::kPerspective, {"perspective_angle", "perspective_dist"}}},
        {"polar2", {kind::kPolar2, {}}},
        {"post_bwraps", {kind::kPostBwraps, {"post_bwraps_cellsize", "post_bwraps_space", "post_bwraps_gain",
                                              "post_bwraps_inner_twist", "post_bwraps_outer_twist"}}},
        {"post_curl", {kind::kPostCurl, {"post_curl_c1", "post_curl_c2"}}},
        {"post_curl3D", {kind::kPostCurl3D, {"post_curl3D_cx", "post_curl3D_cy", "post_curl3D_cz"}}},
        {"pre_bwraps", {kind::kPreBwraps, {"pre_bwraps_cellsize", "pre_bwraps_space", "pre_bwraps_gain",
                                            "pre_bwraps_inner_twist", "pre_bwraps_outer_twist"}}},
        {"pre_disc", {kind::kPreDisc, {}}},
        {"rings", {kind::kRings, {}}},
        {"rings2", {kind::kRings2, {"rings2_val"}}},
        {"scry", {kind::kScry, {}}},
        {"waves", {kind::kWaves, {}}},
        {"wedge", {kind::kWedge, {"wedge_angle", "wedge_hole", "wedge_count", "wedge_swirl"}}},
        {"arch", {kind::kArch, {}}},
        {"blade", {kind::kBlade, {}}},
        {"blur_circle", {kind::kBlurCircle, {}}},
        {"blur_pixelize", {kind::kBlurPixelize, {"blur_pixelize_size", "blur_pixelize_scale"}}},
        {"blur_zoom", {kind::kBlurZoom, {"blur_zoom_length", "blur_zoom_x", "blur_zoom_y"}}},
        {"crop", {kind::kCrop, {"crop_left", "crop_top", "crop_right", "crop_bottom", "crop_scatter_area",
                                 "crop_zero"}}},
        {"post_crop", {kind::kPostCrop, {"post_crop_left", "post_crop_top", "post_crop_right",
                                          "post_crop_bottom", "post_crop_scatter_area", "post_crop_zero"}}},
        {"pre_crop", {kind::kPreCrop, {"pre_crop_left", "pre_crop_top", "pre_crop_right", "pre_crop_bottom",
                                        "pre_crop_scatter_area", "pre_crop_zero"}}},
        {"curl", {kind::kCurl, {"curl_c1", "curl_c2"}}},
        {"rectangles", {kind::kRectangles, {"rectangles_x", "rectangles_y"}}},
        {"epispiral", {kind::kEpispiral, {"epispiral_n", "epispiral_thickness", "epispiral_holes"}}},
        {"pie", {kind::kPie, {"pie_slices", "pie_rotation", "pie_thickness"}}},
        {"rays", {kind::kRays, {}}},
        {"twintrian", {kind::kTwintrian, {}}},
        {"julian", {kind::kJulian, {"julian_power", "julian_dist"}}},
        {"juliascope", {kind::kJuliaScope, {"juliascope_power", "juliascope_dist"}}},
        {"julia3Dz", {kind::kJulia3Dz, {"julia3Dz_power"}}},
        {"julia3D", {kind::kJulia3Djf, {"julia3D_power"}}},
        {"radial_blur", {kind::kRadialBlur, {"radial_blur_angle"}}},
        {"falloff2", {kind::kFalloff2, {"falloff2_scatter", "falloff2_mindist", "falloff2_mul_x",
                                         "falloff2_mul_y", "falloff2_mul_z", "falloff2_mul_c", "falloff2_x0",
                                         "falloff2_y0", "falloff2_z0", "falloff2_invert", "falloff2_type"}}},
        {"post_falloff2", {kind::kPostFalloff2, {"post_falloff2_scatter", "post_falloff2_mindist",
                                                  "post_falloff2_mul_x", "post_falloff2_mul_y",
                                                  "post_falloff2_mul_z", "post_falloff2_mul_c",
                                                  "post_falloff2_x0", "post_falloff2_y0", "post_falloff2_z0",
                                                  "post_falloff2_invert", "post_falloff2_type"}}},
        {"pre_falloff2", {kind::kPreFalloff2, {"pre_falloff2_scatter", "pre_falloff2_mindist",
                                                "pre_falloff2_mul_x", "pre_falloff2_mul_y",
                                                "pre_falloff2_mul_z", "pre_falloff2_mul_c", "pre_falloff2_x0",
                                                "pre_falloff2_y0", "pre_falloff2_z0", "pre_falloff2_invert",
                                                "pre_falloff2_type"}}},
    };
    return table;
}

} // namespace

const RegisteredVarInfo* lookupRegisteredVarKind(const std::string& name) {
    const auto& table = registeredVarTable();
    const auto it = table.find(name);
    return it == table.end() ? nullptr : &it->second;
}

bool isVariationNameGpuEligible(const std::string& name) {
    for (const std::string& localName : VariationRegistry::localVarNames()) {
        if (localName == name) return true;
    }
    return lookupRegisteredVarKind(name) != nullptr;
}

} // namespace apo::gpu
