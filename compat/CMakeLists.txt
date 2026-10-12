# iwd2r. Distributed under LICENSE.md.
add_library(iwd2r_compat STATIC version.cpp)
target_include_directories(iwd2r_compat PUBLIC "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
iwd2r_strict_warnings(iwd2r_compat)
