# iwd2r. Distributed under LICENSE.md.
add_library(iwd2r_platform STATIC platform_version.cpp)
target_include_directories(iwd2r_platform PUBLIC "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iwd2r_platform PUBLIC iwd2r_compat SDL3::SDL3-static miniaudio)
iwd2r_strict_warnings(iwd2r_platform)
