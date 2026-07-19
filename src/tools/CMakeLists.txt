add_executable(apo_render_cli apo_render_cli.cpp)
target_link_libraries(apo_render_cli PRIVATE apo_core)

# B0's points-per-second benchmark harness (FOLLOWUP_PLAN.txt Part B) - a
# manually-run tool, not wired into ctest (see tests/CMakeLists.txt and this
# file's own placement above enable_testing() in the top-level
# CMakeLists.txt), same as apo_render_cli above.
add_executable(apo_bench apo_bench.cpp)
target_link_libraries(apo_bench PRIVATE apo_core)
