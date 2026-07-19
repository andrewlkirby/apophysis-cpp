# Phase 8 (Packaging): produces a self-contained, redistributable build of
# apo_gui - no Qt/vcpkg/Visual Studio installation required on the target
# machine. Deliberately opt-in (a `deploy`/`package_zip` target, not a
# POST_BUILD step on apo_gui itself) so the normal edit-build-test inner
# loop used throughout this port never pays this cost - only run it when
# actually producing a build to hand to someone else.
#
# Usage: cmake --build build --config Release --target package_zip
# Produces build/deploy/ (a runnable, standalone copy of the app) and
# build/apophysis7x-<version>-win64.zip (the same thing, zipped).
#
# Included from the top-level CMakeLists.txt after add_subdirectory(src/ui)
# (needs the apo_gui target to already exist).

set(APO_VERSION "1.0.0" CACHE STRING "Apophysis 7X (C++ port) version, used for the package filename")

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
    # .../VC/Tools/MSVC/<version>/bin/Hostx64/x64 -> .../VC/Redist/MSVC/<version>/x64/Microsoft.VC*.CRT
    get_filename_component(APO_MSVC_TOOLSET_DIR "${APO_MSVC_BIN_DIR}/../../.." ABSOLUTE)
    get_filename_component(APO_MSVC_VERSION "${APO_MSVC_TOOLSET_DIR}" NAME)
    get_filename_component(APO_VC_DIR "${APO_MSVC_TOOLSET_DIR}/../../.." ABSOLUTE)
    file(GLOB APO_CRT_REDIST_DIR "${APO_VC_DIR}/Redist/MSVC/${APO_MSVC_VERSION}/x64/Microsoft.VC*.CRT")
endif()

set(APO_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

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

add_custom_target(package_zip
    COMMAND ${CMAKE_COMMAND} -E tar cf "${CMAKE_BINARY_DIR}/apophysis7x-${APO_VERSION}-win64.zip" --format=zip -- .
    WORKING_DIRECTORY "${APO_DEPLOY_DIR}"
    COMMENT "Zipping ${APO_DEPLOY_DIR} -> apophysis7x-${APO_VERSION}-win64.zip"
    VERBATIM
)
add_dependencies(package_zip deploy)
