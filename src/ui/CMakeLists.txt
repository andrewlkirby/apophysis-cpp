# OBJECT library (not compiled straight into apo_gui) so
# tests/ui/triangle_canvas_interaction_test.cpp can link the exact same
# compiled widget code apo_gui ships, rather than either duplicating the
# source list in a second target or - worse - only being able to test
# through the full apo_gui.exe process (screenshots, not real assertions).
add_library(apo_ui OBJECT
    MainWindow.h
    MainWindow.cpp
    PreviewLabel.h
    PreviewLabel.cpp
    PreviewSizing.h
    RenderWorker.h
    ThumbnailTask.h
    TriangleCanvas.h
    TriangleCanvas.cpp
    EditorWindow.h
    EditorWindow.cpp
    SliderSpin.h
    SliderSpin.cpp
    AdjustDialog.h
    AdjustDialog.cpp
    AdjustPanel.h
    AdjustPanel.cpp
    RenderDialog.h
    RenderDialog.cpp
    RenderAllDialog.h
    RenderAllDialog.cpp
    AppSettings.h
    AppSettings.cpp
    MemoryBudget.h
    MemoryBudget.cpp
    OptionsDialog.h
    OptionsDialog.cpp
    MutateDialog.h
    MutateDialog.cpp
    GradientBrowserDialog.h
    GradientBrowserDialog.cpp
    SmoothPaletteDialog.h
    SmoothPaletteDialog.cpp
    AboutDialog.h
    AboutDialog.cpp
    FullscreenView.h
    FullscreenView.cpp
    CurvesWidget.h
    CurvesWidget.cpp
    CurvesDialog.h
    CurvesDialog.cpp
    PostProcessDialog.h
    PostProcessDialog.cpp
    XaosDialog.h
    XaosDialog.cpp
    ForceSymmetryDialog.h
    ForceSymmetryDialog.cpp
    TransformPanel.h
    TransformPanel.cpp
    GpuCompatibilityBadge.h
    GpuCompatibilityBadge.cpp
    WindowGeometry.h
    UpdateChecker.h
    UpdateChecker.cpp
)
target_include_directories(apo_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(apo_ui PUBLIC apo_core Qt6::Widgets Qt6::Network)

# AppVersion.h.in -> AppVersion.h (APO_VERSION substituted in) so C++ code
# (MainWindow's Check for Updates, AboutDialog) can read the same version
# CMakeLists.txt/Packaging.cmake/release.yml already use, instead of a second
# hand-maintained copy going stale the way AboutDialog's old hardcoded
# "1.0.0" string already had. Configured at CMake-configure time (not
# build time), so it picks up -DAPO_VERSION=... the same way
# MACOSX_BUNDLE_BUNDLE_VERSION above does.
configure_file(AppVersion.h.in "${CMAKE_CURRENT_BINARY_DIR}/AppVersion.h" @ONLY)

add_executable(apo_gui WIN32 main.cpp)
if(WIN32)
    # The .rc/.ico icon resource is a Windows-only concept (no RC compiler
    # exists on a GCC/Clang toolchain) - WIN32 above is harmless off-Windows
    # (it's simply ignored), but this source must stay out of the target
    # entirely on Linux/macOS. This is what gives apo_gui.exe itself its
    # icon in Explorer/Alt-Tab - it does NOT make a *running* window's
    # taskbar button use that icon (that's a separate WM_SETICON concern,
    # controlled by Qt's own QApplication::setWindowIcon() call in main.cpp,
    # which needs the same picture available as an in-binary Qt resource -
    # see app_icon below).
    target_sources(apo_gui PRIVATE resources/app.rc)
endif()
# Embeds the same app.ico app.rc uses for the .exe's own icon (single
# source of truth - also what packaging/linux-appimage.sh derives its PNG
# icon from) as a Qt resource, so main.cpp can hand it to
# QApplication::setWindowIcon() without depending on any file being present
# on disk next to the installed binary. qt_add_resources() (not a checked-in
# .qrc file - unnecessary indirection for a single asset) generates the
# resource .cpp straight from FILES below and adds it to apo_gui directly;
# it works without CMAKE_AUTORCC being enabled project-wide. Resulting
# resource path: ":/resources/app.ico" (PREFIX "/" + the FILES path, taken
# relative to this directory).
qt_add_resources(apo_gui "app_icon"
    PREFIX "/"
    FILES resources/app.ico
)
if(APPLE)
    # Makes apo_gui a proper .app bundle instead of a bare Unix executable -
    # required for macdeployqt (cmake/Packaging.cmake) to have somewhere to
    # embed the Qt frameworks/plugins, and for Finder/Dock/Gatekeeper to
    # treat it as a normal Mac application rather than a CLI tool.
    set_target_properties(apo_gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Apophysis 7X"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.apophysis.apophysis7x"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APO_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APO_VERSION}"
    )
endif()
# apo_core is an OBJECT library too (see src/core/CMakeLists.txt), and
# CMake does NOT transitively flow an OBJECT library's .obj files through
# a second OBJECT library - target_link_libraries(apo_ui PUBLIC apo_core)
# only propagates apo_core's usage requirements (include dirs, Threads,
# pugixml, PNG) to apo_ui, not apo_core's actual object files. apo_core
# must be linked directly into every final (non-object) target that needs
# its symbols.
target_link_libraries(apo_gui PRIVATE apo_ui apo_core)

# WIN32 above suppresses the console window for normal use, but that also
# swallows stdout/stderr - inconvenient while this is still under active
# development and debug output (or a crash message) is often the first
# thing worth seeing. Qt itself still routes qDebug/qWarning to the
# Windows debug output stream regardless; this only affects a plain
# console window's presence.
