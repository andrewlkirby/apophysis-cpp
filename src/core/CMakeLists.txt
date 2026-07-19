# variations/*.cpp files self-register with VariationRegistry via a
# file-scope static initializer (see VariationRegistration.h) - nothing
# else in the program references their symbols directly. A STATIC library
# archive is free to drop object files nothing references when linking the
# final binary (both MSVC's link.exe and GNU ld do this), which would
# silently drop variations. OBJECT libraries don't get archived/pruned -
# every .obj unconditionally flows into whatever links apo_core - so that's
# used here instead, specifically to keep the self-registration pattern
# reliable without needing per-file forced-reference boilerplate.
file(GLOB APO_VARIATION_SOURCES CONFIGURE_DEPENDS variations/*.cpp variations/*.h)

# Each plugins/generated/Plugin<Name>_impl.c compiles one legacy
# Plugin/*.c file (see plugins/thirdparty/) as C, with its symbols
# macro-prefixed unique to that plugin - see PluginVariation.h. The paired
# Plugin<Name>.cpp declares those prefixed symbols extern "C" and registers
# a PluginVariation for them. Both must flow into the same OBJECT library
# (see the comment below on why OBJECT, not STATIC) as the native
# variations.
file(GLOB APO_PLUGIN_SOURCES CONFIGURE_DEPENDS plugins/generated/*.cpp plugins/generated/*.c plugins/*.h)

add_library(apo_core OBJECT
    Variation.h
    VariationRegistry.h
    VariationRegistry.cpp
    VariationRegistration.h
    Rng.h
    Point3.h
    Bezier.h
    Bezier.cpp
    XForm.h
    XForm.cpp
    ColorMap.h
    ColorMap.cpp
    BuiltinGradients.h
    BuiltinGradients.cpp
    Flame.h
    Flame.cpp
    ${APO_VARIATION_SOURCES}
    plugins/PluginVariation.h
    plugins/PluginVariation.cpp
    ${APO_PLUGIN_SOURCES}
    render/Renderer.h
    render/Renderer.cpp
    render/AutoFrame.h
    render/AutoFrame.cpp
    io/PngWriter.h
    io/PngWriter.cpp
    io/FlameIO.h
    io/FlameIO.cpp
    io/GradientIO.h
    io/GradientIO.cpp
    io/ImageCompare.h
    io/ImageCompare.cpp
    edit/TriangleGeometry.h
    edit/TriangleGeometry.cpp
    edit/GradientOps.h
    edit/GradientOps.cpp
    edit/SmoothPalette.h
    edit/SmoothPalette.cpp
    edit/FlameOps.h
    edit/FlameOps.cpp
    edit/MutationOps.h
    edit/MutationOps.cpp
    edit/RandomFlame.h
    edit/RandomFlame.cpp
    edit/Symmetry.h
    edit/Symmetry.cpp
)

target_include_directories(apo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# FOLLOWUP_PLAN.txt B7 - see the top-level CMakeLists.txt's own comment on
# APO_CORE_ENABLE_AVX2 for the full rationale (off by default, deliberately
# never paired with /fp:fast). PRIVATE: this is a codegen choice for
# apo_core's own translation units, not part of its public interface -
# nothing about apo_core's ABI/ headers requires callers to also build
# with AVX2.
if(APO_CORE_ENABLE_AVX2)
    if(MSVC)
        target_compile_options(apo_core PRIVATE /arch:AVX2)
    else()
        target_compile_options(apo_core PRIVATE -mavx2 -mfma)
    endif()
endif()

# Renderer.cpp uses std::jthread (C++20) and needs threading support linked
# in on platforms/toolchains where that isn't automatic.
find_package(Threads REQUIRED)
target_link_libraries(apo_core PUBLIC Threads::Threads)

# Phase 4 (File I/O): pugixml for .flame/.flam3 XML (replacing
# ParameterIO.pas's regex-based parsing - see io/FlameIO.h), libpng for
# rendered-image output (see io/PngWriter.h). Both pulled in via
# vcpkg.json/manifest mode - configure with
# -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake.
find_package(pugixml CONFIG REQUIRED)
find_package(PNG REQUIRED)
target_link_libraries(apo_core PUBLIC pugixml::pugixml PNG::PNG)
