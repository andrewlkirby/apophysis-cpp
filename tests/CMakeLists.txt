# Each *_test.cpp is its own standalone executable (its own main(), see
# TestHelpers.h) - this lets multiple contributors add a new test file each
# without any of them touching a shared file (and hence without merge
# contention), and CONFIGURE_DEPENDS picks up new files automatically.
file(GLOB APO_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)

foreach(test_source ${APO_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE apo_core)
    # Only baseline_regression_test.cpp actually reads this, but defining
    # it for every test target is harmless and keeps this loop uniform.
    target_compile_definitions(${test_name} PRIVATE APO_BASELINES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/baselines")
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Real Qt-widget interaction tests (mouse/keyboard event simulation) -
# separate CMakeLists.txt since they need apo_ui + Qt6::Test rather than
# just apo_core (see tests/ui/CMakeLists.txt).
add_subdirectory(ui)
