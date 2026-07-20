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
    add_compile_options(/W4 /permissive- /EHsc)
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
find_package(Qt6 COMPONENTS Widgets Test REQUIRED)
set(CMAKE_AUTOMOC ON)

# The prebuilt macOS Qt 6.8.0 binaries (aqt / jurplel/install-qt-action) were
# built against an SDK that still had AGL.framework, so Qt6::Gui's exported
# CMake target bakes in a reference to it. Apple removed AGL.framework
# outright starting with Xcode 26 / macOS 26 "Tahoe" (QTBUG-137687), which
# breaks the final link on newer CI runners even though nothing here asks
# for legacy AGL-based OpenGL. Strip any AGL reference - however Qt encoded
# it (raw "-framework AGL" flag or a full .framework path) - from every Qt
# imported target's link interface; a no-op on toolchains that never had it.
if(APPLE)
    foreach(_qt_target Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
        if(TARGET ${_qt_target})
            foreach(_prop INTERFACE_LINK_LIBRARIES INTERFACE_LINK_OPTIONS)
                get_target_property(_apo_qt_libs ${_qt_target} ${_prop})
                if(_apo_qt_libs)
                    list(FILTER _apo_qt_libs EXCLUDE REGEX "AGL")
                    set_target_properties(${_qt_target} PROPERTIES ${_prop} "${_apo_qt_libs}")
                endif()
            endforeach()
        endif()
    endforeach()
endif()

add_subdirectory(src/core)
add_subdirectory(src/tools)
add_subdirectory(src/ui)

enable_testing()
add_subdirectory(tests)

# Phase 8 (Packaging) - opt-in `deploy`/`package_zip` targets, not part of
# the normal build. Needs the apo_gui target from src/ui above.
if(WIN32)
    include(cmake/Packaging.cmake)
endif()
