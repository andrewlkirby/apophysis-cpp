cmake_minimum_required(VERSION 3.20)
# C is needed alongside CXX for the statically-linked Plugin/*.c wrappers
# (see src/core/plugins/) - each is compiled as real C, not C++, to sidestep
# C++'s stricter rules around tentative definitions/const-initialization
# that the legacy plugin ABI header relies on (see
# src/core/plugins/thirdparty/apoplugin.h's file header for the full
# rationale).
project(Apophysis7XCpp CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # $<COMPILE_LANGUAGE:...>-guarded (docs/GPU_RENDERING_PLAN.md): plain
    # add_compile_options(/W4 /permissive- /EHsc) applies to every language a
    # target compiles, including CUDA once APO_ENABLE_CUDA is ON - nvcc's own
    # command-line parser doesn't understand bare, un-prefixed MSVC-style
    # slash flags the way cl.exe does, and CMake's CUDA support doesn't
    # auto-wrap these three in -Xcompiler=... the way it does for flags it
    # specifically recognizes (e.g. /MD) - so unwrapped, nvcc misparses them
    # as extra positional input-file arguments ("A single input file is
    # required for a non-link phase when an output file is specified" -
    # confirmed against a real build). Excluding CUDA here is correct
    # either way, not just a workaround: these are C++-source warning/
    # exception-handling flags with no meaning for device code.
    add_compile_options(
        $<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:/W4>
        $<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:/permissive->
        $<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:/EHsc>
    )
else()
    add_compile_options(-Wall -Wextra)
endif()

# FOLLOWUP_PLAN.txt B7: opt-in AVX2 codegen for apo_core (the math-heavy
# variation/render hot path - see src/core/CMakeLists.txt for where this is
# actually applied) - off by default since it raises the minimum supported
# CPU to roughly 2013+ (Haswell/Excavator-era), a real compatibility cost
# this project shouldn't impose on every build. Deliberately NOT paired
# with /fp:fast, here or anywhere else in this project: renderBatch's
# poisoned-point bailout and accept-path check (Renderer.cpp) depend on
# NaN/Inf propagating honestly through variation math into std::isfinite -
# /fp:fast licenses the compiler to assume no NaNs/Infs exist at all, which
# can silently break exactly that logic. AVX2 alone, under the default
# /fp:precise MSVC already uses, doesn't license FMA contraction (that
# needs /fp:fast) - see FOLLOWUP_PLAN.txt's own progress log for this item
# for how that was verified, not just assumed.
option(APO_CORE_ENABLE_AVX2 "Compile apo_core with AVX2 codegen (/arch:AVX2) - requires a ~2013+ CPU" OFF)

# docs/GPU_RENDERING_PLAN.md: an optional CUDA-accelerated render backend
# (src/core/render/gpu/), off by default so the normal Windows/macOS/Linux
# CI build (and any dev machine without an NVIDIA GPU/CUDA Toolkit
# installed) is completely unaffected. The CPU renderer (src/core/render/
# Renderer.h) remains the universal, authoritative path regardless of this
# setting; RenderDispatcher picks the GPU path only when this was ON at
# build time, a CUDA device is actually present at runtime, and the flame
# being rendered uses no unported/plugin variation.
option(APO_ENABLE_CUDA "Build the optional CUDA render backend (src/core/render/gpu) - requires the CUDA Toolkit" OFF)
if(APO_ENABLE_CUDA)
    # Must be set before enable_language(CUDA); left overridable via
    # -DCMAKE_CUDA_ARCHITECTURES=... (a pre-existing cache entry counts as
    # "defined" and skips this default). Covers Turing (RTX 20-series) /
    # Ampere (RTX 30-series) / Ada (RTX 40-series) - narrower than "every
    # CUDA GPU ever made" but a reasonable default for a 2020s dev machine;
    # widen or use CMake 3.24+'s "native" for the actual build host's GPU.
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES 75 86 89)
    endif()
    if(MSVC)
        # nvcc maintains an explicit allowlist of "supported" MSVC versions
        # and hard-errors (host_config.h's own #error) on anything newer,
        # even though a newer-than-listed cl.exe compiles this project's
        # plain C++20 device/host code without issue in practice - this is
        # nvcc being conservative about host compilers it hasn't been
        # validated against yet, not a real incompatibility. Must be set
        # before enable_language(CUDA) below: that call's own compiler-id
        # detection is itself a CUDA compile and hits the same check.
        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -allow-unsupported-compiler")
    endif()
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    find_package(CUDAToolkit REQUIRED)
endif()

# Phase 0 note: Phase 1's core data model is deliberately dependency-free
# (builds and tests with nothing but a C++20 compiler); libpng/pugixml
# arrived in Phase 4 via vcpkg.json (manifest mode - configure with
# -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake).
#
# Qt6 (Phase 6) is NOT in vcpkg.json: building Qt from source via vcpkg
# takes hours, whereas a prebuilt official Qt SDK (installed here via
# `pip install aqtinstall && python -m aqt install-qt windows desktop
# 6.8.0 win64_msvc2022_64 -O C:\Qt`) is a ~1-2GB download that's ready in
# under a minute. Point CMake at it with
# -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64 (or wherever it's installed).
find_package(Qt6 COMPONENTS Widgets Network Test REQUIRED)
set(CMAKE_AUTOMOC ON)

# Declared here (rather than in cmake/Packaging.cmake) so src/ui's
# MACOSX_BUNDLE_*_VERSION properties can use it too - both need the same
# value, and src/ui is added before Packaging.cmake is included below.
set(APO_VERSION "1.0.0" CACHE STRING "Apophysis 7X (C++ port) version, used for package filenames and bundle metadata")

add_subdirectory(src/core)
add_subdirectory(src/tools)
add_subdirectory(src/ui)

enable_testing()
add_subdirectory(tests)

# Phase 8 (Packaging) - opt-in `deploy`/`package_zip` targets, not part of
# the normal build. Needs the apo_gui target from src/ui above. Linux has
# no equivalent CMake target: its packaging tool (linuxdeploy) isn't part
# of the Qt SDK like windeployqt/macdeployqt are, so it's fetched over the
# network in CI instead - see packaging/linux-appimage.sh.
if(WIN32 OR APPLE)
    include(cmake/Packaging.cmake)
endif()
