# iwd2r. Distributed under LICENSE.md.
find_package(Threads REQUIRED)
# miniaudio's implementation: third-party code, so not held to the strict
# warning level (MSVC /W4 warns in it).
add_library(iwd2r_miniaudio STATIC miniaudio_impl.cpp)
target_link_libraries(iwd2r_miniaudio PUBLIC Threads::Threads miniaudio ${CMAKE_DL_LIBS})
add_library(iwd2r_platform STATIC platform_version.cpp threads.cpp clock.cpp fs.cpp data_dir.cpp ddraw_shim.cpp dsound_shim.cpp present.cpp window.cpp window_keys.cpp win32_stubs.cpp gl_compat.cpp affinity.cpp)
target_include_directories(iwd2r_platform PUBLIC "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iwd2r_platform PUBLIC Threads::Threads iwd2r_compat SDL3::SDL3-static miniaudio iwd2r_miniaudio)
iwd2r_strict_warnings(iwd2r_platform)

set_property(SOURCE win32_stubs.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    "IWD2R_VERSION_STRING=\"${IWD2R_VERSION_STRING}\"")
