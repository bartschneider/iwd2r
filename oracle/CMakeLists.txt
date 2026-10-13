# The function oracle. The PE parser is portable (its tests run everywhere);
# the runner that maps the vanilla iwd2.exe at 0x400000 exists only in 32-bit
# Windows builds.
add_library(iwd2r_oracle_pe STATIC loader/pe.cpp)
target_include_directories(iwd2r_oracle_pe PUBLIC "${PROJECT_SOURCE_DIR}")
iwd2r_strict_warnings(iwd2r_oracle_pe)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # vanilla.json -> vanilla_config.h, so the runner needs no JSON parser.
    set(vanilla_json "${CMAKE_CURRENT_SOURCE_DIR}/loader/vanilla.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${vanilla_json}")
    file(READ "${vanilla_json}" json)
    foreach(key md5 winmain_call_site winmain srand rand getptd)
        string(JSON VANILLA_${key} GET "${json}" ${key})
    endforeach()
    string(JSON VANILLA_tiddata_rand_start GET "${json}" tiddata_rand 0)
    string(JSON VANILLA_tiddata_rand_end GET "${json}" tiddata_rand 1)
    string(JSON range_count LENGTH "${json}" crt_ranges)
    math(EXPR range_last "${range_count} - 1")
    set(VANILLA_crt_ranges "")
    foreach(i RANGE ${range_last})
        string(JSON range_start GET "${json}" crt_ranges ${i} 0)
        string(JSON range_end GET "${json}" crt_ranges ${i} 1)
        string(APPEND VANILLA_crt_ranges "    { ${range_start}u, ${range_end}u },\n")
    endforeach()
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/vanilla_config.h" @ONLY CONTENT
"// Generated from oracle/loader/vanilla.json by oracle/CMakeLists.txt.
#pragma once
#include <cstdint>
constexpr const char* kVanillaMd5 = \"@VANILLA_md5@\";
constexpr uint32_t kVanillaWinMainCallSite = @VANILLA_winmain_call_site@u;
constexpr uint32_t kVanillaWinMain = @VANILLA_winmain@u;
constexpr uint32_t kVanillaSrand = @VANILLA_srand@u;
constexpr uint32_t kVanillaRand = @VANILLA_rand@u;
constexpr uint32_t kVanillaGetptd = @VANILLA_getptd@u;
// [start, end) offsets of the rand state in the thread's _tiddata.
constexpr uint32_t kVanillaTiddataRandStart = @VANILLA_tiddata_rand_start@u;
constexpr uint32_t kVanillaTiddataRandEnd = @VANILLA_tiddata_rand_end@u;
// [start, end) ranges of the CRT's own bookkeeping in .data.
constexpr uint32_t kVanillaCrtRanges[][2] = {
@VANILLA_crt_ranges@};
")

    # The runner's own image sits at a fixed 0x10000000, out of the vanilla
    # range. It links the CRT statically: no runtime DLLs to deploy beside it,
    # and fewer modules competing for low addresses.
    # Case files register themselves (ORACLE_CASE); add new ones here.
    add_executable(iwd2r_oracle runner/oracle_main.cpp runner/snapshot.cpp runner/report.cpp
        loader/pe.cpp loader/map_win32.cpp cases/selftest.cpp)
    target_include_directories(iwd2r_oracle PRIVATE "${PROJECT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(iwd2r_oracle PRIVATE bcrypt)
    target_link_options(iwd2r_oracle PRIVATE /BASE:0x10000000 /DYNAMICBASE:NO /FIXED)
    set_property(TARGET iwd2r_oracle PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    iwd2r_strict_warnings(iwd2r_oracle)
endif()
