# iwd2r. Distributed under LICENSE.md.
add_executable(iwd2r_tests test_sanity.cpp test_win_types.cpp test_win_types_minmax.cpp test_cstring.cpp test_geometry.cpp test_list.cpp test_array_map.cpp test_sync.cpp test_file.cpp test_misc_mfc.cpp test_ini.cpp test_clock.cpp test_fs.cpp test_data_dir.cpp test_ddraw.cpp test_dsound.cpp test_window.cpp test_network.cpp test_version_affinity.cpp test_png.cpp test_script.cpp smoke_start_screen.cpp scenario_start_screen.cpp test_layout.cpp $<TARGET_OBJECTS:iwd2r_engine>)
target_include_directories(iwd2r_tests PRIVATE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/compat")
# Engine headers (test_network.cpp) are not held to the strict warning level.
target_include_directories(iwd2r_tests SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/src" ${ZLIB_INCLUDE_DIRS})
target_link_libraries(iwd2r_tests PRIVATE iwd2r_compat iwd2r_platform ${ZLIB_LIBRARIES} Catch2::Catch2WithMain)
iwd2r_strict_warnings(iwd2r_tests)
add_dependencies(iwd2r_tests iwd2r)
target_compile_definitions(iwd2r_tests PRIVATE IWD2R_EXE="$<TARGET_FILE:iwd2r>"
    IWD2R_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    "IWD2R_VERSION_STRING=\"${IWD2R_VERSION_STRING}\"")
catch_discover_tests(iwd2r_tests PROPERTIES SKIP_REGULAR_EXPRESSION "\\[iwd2r-skip\\]" TIMEOUT 180)
