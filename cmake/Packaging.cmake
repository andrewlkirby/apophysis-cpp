# Phase 8 (Packaging): produces a self-contained, redistributable build of
# apo_gui - no Qt/vcpkg/Visual Studio installation required on the target
# machine. Deliberately opt-in (a `deploy`/`package_zip` target, not a
# POST_BUILD step on apo_gui itself) so the normal edit-build-test inner
# loop used throughout this port never pays this cost - only run it when
# actually producing a build to hand to someone else.
#
# Usage: cmake --build build --config Release --target package_zip
#
# Windows produces build/deploy/ (a runnable, standalone copy of the app)
# and build/apophysis7x-<version>-win64.zip (the same thing, zipped).
# macOS produces build/deploy/Apophysis 7X.app and
# build/apophysis7x-<version>-macos-<arch>.zip (the .app, zipped) - <arch>
# is CMAKE_SYSTEM_PROCESSOR (arm64 or x86_64) rather than a hardcoded
# value, since this project doesn't set CMAKE_OSX_ARCHITECTURES and so
# always builds natively for whatever Mac (Apple Silicon or Intel) runs
# the build - install.sh matches this against its own `uname -m`.
#
# Included from the top-level CMakeLists.txt after add_subdirectory(src/ui)
# (needs the apo_gui target to already exist). APO_VERSION itself is
# declared in the top-level CMakeLists.txt, not here, since src/ui's
# MACOSX_BUNDLE_*_VERSION properties need it too and src/ui is added
# before this file is included.

set(APO_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

if(APPLE)
    # macdeployqt lives alongside windeployqt/qmake in the SDK's bin/ dir -
    # same Qt6_DIR-relative lookup as the Windows branch below.
    get_filename_component(APO_QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(APO_MACDEPLOYQT_EXECUTABLE
        NAMES macdeployqt
        HINTS "${APO_QT_BIN_DIR}"
    )

    add_custom_target(deploy
        COMMENT "Staging a standalone Apophysis 7X.app in ${APO_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${APO_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APO_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_BUNDLE_DIR:apo_gui>"
            "${APO_DEPLOY_DIR}/Apophysis 7X.app"
        # -always-overwrite: macdeployqt otherwise skips frameworks it
        # thinks are already up to date, which is wrong the first time it
        # runs against a bundle copy that was just freshly staged above.
        COMMAND "${APO_MACDEPLOYQT_EXECUTABLE}"
            "${APO_DEPLOY_DIR}/Apophysis 7X.app"
            -always-overwrite
        VERBATIM
    )
    add_dependencies(deploy apo_gui)

    add_custom_target(package_zip
        # ditto (not `cmake -E tar`) preserves the .app bundle's symlinks
        # (Contents/MacOS/... etc.) - CMake's own zip writer doesn't.
        COMMAND ditto -c -k --sequesterRsrc --keepParent
            "Apophysis 7X.app"
            "${CMAKE_BINARY_DIR}/apophysis7x-${APO_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}.zip"
        WORKING_DIRECTORY "${APO_DEPLOY_DIR}"
        COMMENT "Zipping ${APO_DEPLOY_DIR}/Apophysis 7X.app -> apophysis7x-${APO_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}.zip"
        VERBATIM
    )
    add_dependencies(package_zip deploy)

    return()
endif()

# --- Windows -----------------------------------------------------------

# windeployqt lives in the same bin/ directory as qmake/moc etc. - Qt6_DIR
# points at .../lib/cmake/Qt6, so climb back to the SDK root's bin/.
get_filename_component(APO_QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
find_program(APO_WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt6 windeployqt
    HINTS "${APO_QT_BIN_DIR}"
)

# The MSVC toolset's redistributable CRT DLLs (vcruntime140.dll,
# msvcp140.dll, ...) - windeployqt's own --compiler-runtime flag needs
# VCINSTALLDIR set to find these reliably (see the flag's own behavior:
# without it, windeployqt silently skips CRT deployment instead of
# erroring, which is easy to miss - verified directly by reproducing the
# resulting crash: a packaged build launched with no VC++ Redistributable
# on the target machine fails inside Qt6Core.dll with a raw buffer-overrun
# exception, not a clean "missing DLL" error). Locating them here instead
# means the deploy step doesn't depend on VCINSTALLDIR being set in
# whatever shell invokes cmake --build.
if(MSVC)
    get_filename_component(APO_MSVC_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    # .../VC/Tools/MSVC/<version>/bin/Hostx64/x64 -> .../VC/Redist/MSVC/<redist-version>/x64/Microsoft.VC*.CRT
    get_filename_component(APO_MSVC_TOOLSET_DIR "${APO_MSVC_BIN_DIR}/../../.." ABSOLUTE)
    get_filename_component(APO_MSVC_VERSION "${APO_MSVC_TOOLSET_DIR}" NAME)
    get_filename_component(APO_VC_DIR "${APO_MSVC_TOOLSET_DIR}/../../.." ABSOLUTE)
    # Redist/MSVC/<version>/'s version doesn't always exactly match
    # Tools/MSVC/<version>/'s - confirmed on a real VS2022 BuildTools-only
    # install: Tools had 14.44.35207, Redist only had 14.44.35112 (the
    # redist package's own release cadence lags the compiler toolset's).
    # An exact-version glob then silently matches nothing, APO_CRT_REDIST_DIR
    # stays unset, and this whole CRT-bundling step becomes a silent no-op -
    # the resulting zip still launches fine on a dev machine that already
    # has the redistributable installed system-wide, then fails with a
    # missing-DLL error on one that doesn't, with no warning at build time.
    # Match on just the major.minor ABI band (e.g. "14.44") instead - that's
    # what actually determines CRT binary compatibility, and the
    # patch/build component is safe to let differ between the compiler and
    # its redistributable package.
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" APO_MSVC_ABI_VERSION "${APO_MSVC_VERSION}")
    file(GLOB APO_CRT_REDIST_CANDIDATES "${APO_VC_DIR}/Redist/MSVC/${APO_MSVC_ABI_VERSION}.*/x64/Microsoft.VC*.CRT")
    if(APO_CRT_REDIST_CANDIDATES)
        # Prefer the highest patch/build version if more than one is
        # installed side-by-side (lexical sort matches numeric order here:
        # every observed Redist patch/build component is the same digit
        # width within one VS release).
        list(SORT APO_CRT_REDIST_CANDIDATES)
        list(GET APO_CRT_REDIST_CANDIDATES -1 APO_CRT_REDIST_DIR)
    endif()
endif()

add_custom_target(deploy
    COMMENT "Staging a standalone apo_gui build in ${APO_DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${APO_DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${APO_DEPLOY_DIR}"
    # apo_gui.exe plus its already-resolved vcpkg DLLs (libpng/pugixml/zlib -
    # copied alongside it automatically by vcpkg's VCPKG_APPLOCAL_DEPS,
    # on by default in manifest mode).
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:apo_gui>"
        "${APO_DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} "-DSRC_DIR=$<TARGET_FILE_DIR:apo_gui>" "-DDST_DIR=${APO_DEPLOY_DIR}" "-DPATTERN=*.dll"
        -P "${CMAKE_CURRENT_LIST_DIR}/CopyMatching.cmake"
    COMMAND "${APO_WINDEPLOYQT_EXECUTABLE}"
        "$<IF:$<CONFIG:Debug>,--debug,--release>"
        --no-translations
        --compiler-runtime
        "${APO_DEPLOY_DIR}/apo_gui.exe"
    VERBATIM
)
add_dependencies(deploy apo_gui)

if(APO_CRT_REDIST_DIR)
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} "-DSRC_DIR=${APO_CRT_REDIST_DIR}" "-DDST_DIR=${APO_DEPLOY_DIR}" "-DPATTERN=*.dll"
            -P "${CMAKE_CURRENT_LIST_DIR}/CopyMatching.cmake"
        COMMENT "Bundling the MSVC redistributable CRT DLLs"
        VERBATIM
    )
endif()

# docs/GPU_RENDERING_PLAN.md: apo_gui dynamically links CUDA::cudart
# (src/core/CMakeLists.txt) whenever APO_ENABLE_CUDA is ON, but neither
# windeployqt nor the CRT-redist step above knows anything about it - a
# deploy/package_zip build made with CUDA enabled would otherwise launch
# fine here (cudart64_*.dll already resolves from the CUDA Toolkit install
# on this machine) and then fail with a "code execution cannot proceed
# because <dll> was not found" popup on any target machine that doesn't
# happen to have the CUDA Toolkit installed - the exact class of error this
# comment is here to prevent. CUDAToolkit_BIN_DIR comes from find_package
# (CUDAToolkit) in the top-level CMakeLists.txt.
if(APO_ENABLE_CUDA)
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} "-DSRC_DIR=${CUDAToolkit_BIN_DIR}" "-DDST_DIR=${APO_DEPLOY_DIR}" "-DPATTERN=cudart64_*.dll"
            -P "${CMAKE_CURRENT_LIST_DIR}/CopyMatching.cmake"
        COMMENT "Bundling the CUDA runtime DLL (cudart64_*.dll)"
        VERBATIM
    )
endif()

add_custom_target(package_zip
    COMMAND ${CMAKE_COMMAND} -E tar cf "${CMAKE_BINARY_DIR}/apophysis7x-${APO_VERSION}-win64.zip" --format=zip -- .
    WORKING_DIRECTORY "${APO_DEPLOY_DIR}"
    COMMENT "Zipping ${APO_DEPLOY_DIR} -> apophysis7x-${APO_VERSION}-win64.zip"
    VERBATIM
)
add_dependencies(package_zip deploy)
