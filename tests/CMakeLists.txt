# iwd2r. Distributed under LICENSE.md.
add_executable(iwd2r_tests test_sanity.cpp test_win_types.cpp test_win_types_minmax.cpp test_cstring.cpp test_geometry.cpp)
target_include_directories(iwd2r_tests PRIVATE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/compat")
target_link_libraries(iwd2r_tests PRIVATE iwd2r_compat iwd2r_platform Catch2::Catch2WithMain)
iwd2r_strict_warnings(iwd2r_tests)
catch_discover_tests(iwd2r_tests)
