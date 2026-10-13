find_package(Threads REQUIRED)
# miniaudio's implementation: third-party code, so not held to the strict
# warning level (MSVC /W4 warns in it).
add_library(iwd2r_miniaudio STATIC miniaudio_impl.cpp)
target_link_libraries(iwd2r_miniaudio PUBLIC Threads::Threads miniaudio ${CMAKE_DL_LIBS})
# The presenter's SPIR-V blobs (platform/shaders/*.spv, made by
# tools/shaders.py and committed) as byte arrays, so builds never need glslang.
set(IWD2R_SHADERS quad.vert nearest.frag sharp.frag smooth.frag)
set(shader_table "")
set(shader_arrays "")
foreach(name IN LISTS IWD2R_SHADERS)
    set(spv "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}.spv")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${spv}")
    file(READ "${spv}" hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(MAKE_C_IDENTIFIER "${name}" id)
    string(APPEND shader_arrays "const unsigned char k_${id}[] = { ${bytes} };\n")
    string(APPEND shader_table "    { \"${name}\", k_${id}, sizeof(k_${id}) },\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp.in"
    "// Generated from platform/shaders/*.spv by platform/CMakeLists.txt.\n"
    "#include \"platform/present_backend.h\"\n\n#include <cstring>\n\n"
    "namespace platform::present {\n\nnamespace {\n\n${shader_arrays}\n"
    "struct Entry {\n    const char* name;\n    const unsigned char* code;\n    size_t size;\n};\n\n"
    "const Entry kShaders[] = {\n${shader_table}};\n\n}\n\n"
    "bool ShaderCode(const char* name, const unsigned char** code, size_t* size)\n{\n"
    "    for (const Entry& e : kShaders) {\n        if (std::strcmp(e.name, name) == 0) {\n"
    "            *code = e.code;\n            *size = e.size;\n            return true;\n        }\n    }\n"
    "    return false;\n}\n\n}\n")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp" COPYONLY)
add_library(iwd2r_platform STATIC platform_version.cpp threads.cpp clock.cpp fs.cpp data_dir.cpp ddraw_shim.cpp dsound_shim.cpp compose.cpp present.cpp present_classic.cpp present_gpu.cpp "${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp" window.cpp window_keys.cpp win32_stubs.cpp gl_compat.cpp affinity.cpp png.cpp script_core.cpp script.cpp state_dump.cpp replay.cpp)
target_include_directories(iwd2r_platform PUBLIC "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iwd2r_platform PUBLIC Threads::Threads iwd2r_compat SDL3::SDL3-static miniaudio iwd2r_miniaudio ${ZLIB_LIBRARIES})
# The screenshot writer (png.cpp) uses the vendored zlib.
target_include_directories(iwd2r_platform SYSTEM PRIVATE ${ZLIB_INCLUDE_DIRS})
iwd2r_strict_warnings(iwd2r_platform)

set_property(SOURCE win32_stubs.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    "IWD2R_VERSION_STRING=\"${IWD2R_VERSION_STRING}\"")
