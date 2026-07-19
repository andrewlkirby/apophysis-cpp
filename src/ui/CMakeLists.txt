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
)
target_include_directories(apo_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(apo_ui PUBLIC apo_core Qt6::Widgets)

add_executable(apo_gui WIN32 main.cpp)
if(WIN32)
    # The .rc/.ico icon resource is a Windows-only concept (no RC compiler
    # exists on a GCC/Clang toolchain) - WIN32 above is harmless off-Windows
    # (it's simply ignored), but this source must stay out of the target
    # entirely on Linux/macOS.
    target_sources(apo_gui PRIVATE resources/app.rc)
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
