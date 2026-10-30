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
    foreach(key md5 winmain_call_site winmain srand rand getptd crt_heap frame_hook frame_hook_bytes
            frame_hook_target)
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
// The replay recorder's per-frame hook: CBaldurChitin::SynchronousUpdate, a
// 5-byte jmp (these bytes) to CChitin::SynchronousUpdate.
constexpr uint32_t kVanillaFrameHook = @VANILLA_frame_hook@u;
constexpr const char* kVanillaFrameHookBytes = \"@VANILLA_frame_hook_bytes@\";
constexpr uint32_t kVanillaFrameHookTarget = @VANILLA_frame_hook_target@u;
// The dword holding the CRT's heap handle (_crtheap).
constexpr uint32_t kVanillaCrtHeap = @VANILLA_crt_heap@u;
// [start, end) offsets of the rand state in the thread's _tiddata.
constexpr uint32_t kVanillaTiddataRandStart = @VANILLA_tiddata_rand_start@u;
constexpr uint32_t kVanillaTiddataRandEnd = @VANILLA_tiddata_rand_end@u;
// [start, end) ranges of the CRT's own bookkeeping in .data.
constexpr uint32_t kVanillaCrtRanges[][2] = {
@VANILLA_crt_ranges@};
")

    # The runner's own image sits at a fixed 0x10000000, out of the vanilla
    # range. Our engine's objects are linked in (not entry.cpp), so cases call
    # the original and ours side by side; the engine, the shims and SDL are
    # built against the DLL CRT, so the runner uses it too (the x86 runtime
    # DLLs ship with Windows' VC++ redistributable and Visual Studio). Our
    # static constructors run under that CRT, the original's under its own;
    # they share no state.
    # Case files register themselves (ORACLE_CASE); add new ones here. Files
    # that include engine headers must not include <windows.h> (compat's
    # win_types.h defines the same names): selftest_ours.cpp is the bridge
    # selftest.cpp uses.
    add_executable(iwd2r_oracle runner/oracle_main.cpp runner/snapshot.cpp runner/report.cpp
        loader/pe.cpp loader/map_win32.cpp cases/selftest.cpp cases/selftest_ours.cpp cases/rand.cpp
        cases/tables.cpp cases/pending.cpp cases/rules.cpp cases/save_load.cpp cases/items.cpp cases/vid.cpp cases/vis.cpp cases/spawn.cpp cases/ai.cpp
        cases/area.cpp cases/infinity.cpp cases/move.cpp cases/blood.cpp cases/wind_paladin.cpp cases/combat.cpp cases/ui_misc.cpp
        cases/store_spell.cpp cases/area_tint.cpp cases/messages.cpp cases/projectile.cpp
        cases/spell_list.cpp cases/effects.cpp cases/store_chat.cpp cases/rule_tables.cpp
        cases/timer.cpp cases/anim_types.cpp cases/aliases.cpp cases/chitin.cpp
        cases/store_spell.cpp cases/area_tint.cpp cases/messages_wire.cpp
        $<TARGET_OBJECTS:iwd2r_engine>)
    target_include_directories(iwd2r_oracle PRIVATE "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(iwd2r_oracle PRIVATE iwd2r_platform iwd2r_compat ${ZLIB_LIBRARIES} bcrypt)
    # /NXCOMPAT:NO: the original's exception handlers run from private memory,
    # which x86 handler validation refuses while DEP is on (oracle_main.cpp).
    target_link_options(iwd2r_oracle PRIVATE /BASE:0x10000000 /DYNAMICBASE:NO /FIXED /NXCOMPAT:NO)
    iwd2r_strict_warnings(iwd2r_oracle)

    # The replay recorder (oracle/replay): injector.exe starts the original
    # with recorder.dll loaded into it. fields.json (tools/gen_fields.py, from
    # the headers' offset comments) -> replay_fields.h, so the DLL holds no
    # offset of its own.
    set(fields_json "${CMAKE_CURRENT_SOURCE_DIR}/replay/fields.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${fields_json}")
    file(READ "${fields_json}" fields)
    string(JSON field_count LENGTH "${fields}")
    math(EXPR field_last "${field_count} - 1")
    set(REPLAY_FIELDS "")
    set(REPLAY_SCREENS "")
    foreach(i RANGE ${field_last})
        string(JSON name MEMBER "${fields}" ${i})
        string(JSON base GET "${fields}" ${name} base)
        string(JSON offset GET "${fields}" ${name} offset)
        string(JSON type GET "${fields}" ${name} type)
        string(JSON count ERROR_VARIABLE no_count GET "${fields}" ${name} count)
        if(no_count)
            set(count 1)
        endif()
        if(base MATCHES "^0x")
            set(address "${base}u")
        else()
            set(address "0u")
        endif()
        string(APPEND REPLAY_FIELDS
            "// ${name}: ${base} + ${offset} (${type})\n"
            "constexpr ReplayField kField_${name} = { ${offset}u, ReplayType::k_${type}, ${count}u, ${address} };\n")
        if(name MATCHES "^engine_(.*)$")
            string(APPEND REPLAY_SCREENS "    { \"${CMAKE_MATCH_1}\", kField_${name} },\n")
        endif()
    endforeach()
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/replay_fields.h" @ONLY CONTENT
"// Generated from oracle/replay/fields.json by oracle/CMakeLists.txt.
#pragma once
#include <cstdint>
enum class ReplayType { k_u8, k_i8, k_u16, k_i16, k_u32, k_i32, k_ptr, k_resref, k_size };
struct ReplayField {
    uint32_t offset; // from the object's start; for k_size, the size
    ReplayType type;
    uint32_t count; // elements, 1 unless an array
    uint32_t address; // a global's address, 0 for a class member
};
struct ReplayScreen {
    const char* name;
    ReplayField field; // CBaldurChitin's member pointing at that engine
};
@REPLAY_FIELDS@
constexpr ReplayScreen kReplayScreens[] = {
@REPLAY_SCREENS@};
")
    # The injector: no engine code, the DLL CRT like the runner.
    add_executable(iwd2r_injector replay/injector.cpp)
    set_target_properties(iwd2r_injector PROPERTIES OUTPUT_NAME injector)
    iwd2r_strict_warnings(iwd2r_injector)
    # The DLL links the static CRT, so it brings everything it needs into the
    # game's process and depends on no runtime DLL the game's folder lacks.
    # It compiles the scenario parser and player from platform/script_core.cpp
    # (the engine's own source, which needs nothing else of the platform
    # layer); recorder_script.cpp bridges it, since it includes
    # compat/win_types.h, which must not meet <windows.h> in one file.
    add_library(iwd2r_recorder SHARED replay/recorder.cpp replay/recorder_script.cpp
        "${PROJECT_SOURCE_DIR}/platform/script_core.cpp")
    set_target_properties(iwd2r_recorder PROPERTIES OUTPUT_NAME recorder
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_include_directories(iwd2r_recorder PRIVATE "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")
    iwd2r_strict_warnings(iwd2r_recorder)
endif()
