// recorder.dll: the replay recorder inside the original iwd2.exe (spec 4.6).
// oracle/replay/injector.cpp loads it into the suspended game; DllMain then
// installs every hook before the game's first instruction runs.
//
// Environment (from the injector's CreateProcess environment block):
//   IWD2R_REPLAY_DIR   folder for the dumps, status.txt and recorder.log
//                      (required)
//   IWD2R_SCRIPT       the scenario (platform/script.h's format), optional
//   IWD2R_DUMP_EVERY   a dump every N frames (default 30; 0: none)
//   IWD2R_SEED         the seed for the original srand (default 1)
//   IWD2R_TEST_STEAL_FOCUS=N  test: a window of the recorder's takes the
//                      foreground at frame N
//   IWD2R_TEST_NO_FOCUS_GUARD=1  test: activation messages reach the game
//   IWD2R_TEST_SLOW_READS=MS  test: from the first frame on, every ReadFile
//                      of a thread other than the main one sleeps MS ms
//                      first (a slow disk)
//
// The frame: the original's main thread calls CBaldurChitin::SynchronousUpdate
// once per AI update (vanilla.json "frame_hook"). Its first instruction, a
// 5-byte jmp to CChitin::SynchronousUpdate, becomes a jmp to FrameTrampoline,
// which calls OnFrame and then takes the original jmp. OnFrame first asks
// the frame gate (platform/replay_clock.h FrameGate) whether the call is a
// frame: a call drawn while the AI thread is inside an update (a load
// redrawing its progress) is held, and OnFrame returns at once. For a frame
// it counts the frame, runs the scenario step, writes the due dumps and
// status.txt, and advances the virtual clock. The trampoline then calls
// CChitin::SynchronousUpdate itself and, when it returns, OnFrameEnd fires
// the due timeSetEvent callbacks, which wake the AI thread for the next
// AsynchronousUpdate: the AI update of a frame runs after the main thread's
// frame (its render included), never beside it (platform/replay_clock.h,
// "The order inside a frame"). Dumps are written only in OnFrame, on the
// main thread, while the AI thread waits for that wake-up.
//
// Hooked imports (the exe's IAT): input GetAsyncKeyState, GetCursorPos,
// SetCursorPos, PeekMessageA, GetMessageA; time timeGetTime, GetTickCount,
// timeSetEvent, timeKillEvent, and the wall clock GetLocalTime,
// GetSystemTime, GetTimeZoneInformation (VC6's time() reads them, and the
// original seeds its AI thread's rand with time()); and LoadLibraryA (binkw32 refused, so the
// movies are skipped as in our engine, whose LoadLibraryA returns NULL) and
// MessageBoxA (answered with its first button without showing it, as our
// engine does headless); GetDoubleClickTime, GetSystemMetrics and
// SystemParametersInfoA (the double-click, button-swap, key-repeat and
// wheel settings fixed to our engine's, platform/input_settings.h; other
// indices pass through); RegisterClassExA (the game window's class gets the
// focus guard, GuardProc: activation messages are sent straight to the
// window procedure, past the message hooks) and CreateWindowExA (notes the
// game window); ReadFile only with IWD2R_TEST_SLOW_READS. An import the
// exe does not have is logged and skipped.
// Real keyboard and mouse input never reaches the game: the hooks
// answer from the scenario, and the message hooks drop keyboard and mouse
// messages.
//
// The scenario's shot NAME writes NAME.bmp, the game window's client area.
//
// recorder.log names every hook, the scenario, the checkpoints and anything
// refused. A refusal to start (no IWD2R_REPLAY_DIR, a scenario error, an
// unexpected byte at the hook site) makes DllMain return FALSE, so the
// injector's LoadLibraryA fails and it exits 12.
//
// Built with the static CRT (oracle/CMakeLists.txt): the DLL brings its own
// runtime into the game's process.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mmsystem.h>

#include "oracle/replay/recorder_script.h"
#include "platform/input_settings.h"
#include "platform/replay_clock.h"
#include "replay_fields.h"
#include "vanilla_config.h"

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace {

// The field types the dump reads them as.
static_assert(kField_baldur_chitin.type == ReplayType::k_ptr && kField_baldur_chitin.address != 0, "");
static_assert(kField_active_engine.type == ReplayType::k_ptr, "");
static_assert(kField_au_counter.type == ReplayType::k_i32, "");
static_assert(kField_fullscreen.type == ReplayType::k_u8, "");
static_assert(kField_in_async_update.type == ReplayType::k_i32, "");
static_assert(kField_reinitializing.type == ReplayType::k_u8, "");
static_assert(kField_progress_bar_active.type == ReplayType::k_u8, "");
static_assert(kField_object_game.type == ReplayType::k_ptr, "");
static_assert(kField_objects_array.type == ReplayType::k_ptr, "");
static_assert(kField_objects_max_index.type == ReplayType::k_i16, "");
static_assert(kField_objects_next_id.type == ReplayType::k_i16, "");
static_assert(kField_object_entry_id.type == ReplayType::k_i16, "");
static_assert(kField_object_entry_ptr.type == ReplayType::k_ptr, "");
static_assert(kField_object_entry_size.type == ReplayType::k_size, "");
static_assert(kField_create_char_sprite.type == ReplayType::k_i32, "");
static_assert(kField_create_char_skill_points.type == ReplayType::k_i32, "");
static_assert(kField_create_char_memory_extra.type == ReplayType::k_i32, "");
static_assert(kField_sprite_feats.type == ReplayType::k_u32 && kField_sprite_feats.count == 3, "");
static_assert(kField_sprite_portrait.type == ReplayType::k_resref, "");
static_assert(kField_visible_area.type == ReplayType::k_u8, "");
static_assert(kField_game_areas.type == ReplayType::k_ptr && kField_game_areas.count == 12, "");
static_assert(kField_character_portraits.type == ReplayType::k_i32 && kField_character_portraits.count == 6, "");
static_assert(kField_characters_count.type == ReplayType::k_i16, "");
static_assert(kField_area_resref.type == ReplayType::k_resref, "");
static_assert(kField_sprite_pos.type == ReplayType::k_i32 && kField_sprite_pos.count == 2, "");
static_assert(kField_sprite_hp.type == ReplayType::k_i16, "");
static_assert(kField_sprite_max_hp.type == ReplayType::k_i16, "");
static_assert(kField_sprite_skills.type == ReplayType::k_i8 && kField_sprite_skills.count == 64, "");
static_assert(kField_sprite_action.type == ReplayType::k_i16, "");
static_assert(kField_sprite_modal.type == ReplayType::k_u8, "");
static_assert(kField_sprite_facing.type == ReplayType::k_i16, "");
static_assert(kField_object_type.type == ReplayType::k_u8, "");
static_assert(kField_area_vert_sort.type == ReplayType::k_ptr, "");
static_assert(kField_area_vert_sort_back.type == ReplayType::k_ptr, "");
static_assert(kField_area_vert_sort_flight.type == ReplayType::k_ptr, "");
static_assert(kField_list_node_next.type == ReplayType::k_ptr, "");
static_assert(kField_list_node_data.type == ReplayType::k_ptr, "");
static_assert(kField_area_visibility_map.type == ReplayType::k_ptr, "");
static_assert(kField_area_visibility_size.type == ReplayType::k_i32, "");
static_assert(kField_door_name.type == ReplayType::k_i8 && kField_door_name.count == 32, "");
static_assert(kField_door_flags.type == ReplayType::k_u32, "");
static_assert(kField_dialog_active.type == ReplayType::k_u8, "");
static_assert(kField_dialog_talker.type == ReplayType::k_i32, "");
static_assert(kField_dialog_entry.type == ReplayType::k_u32, "");
static_assert(kField_dialog_waiting.type == ReplayType::k_i32, "");
static_assert(kField_dialog_entries.type == ReplayType::k_ptr, "");
static_assert(kField_dialog_entries_size.type == ReplayType::k_i32, "");
static_assert(kField_dialog_replies.type == ReplayType::k_ptr, "");
static_assert(kField_dialog_replies_size.type == ReplayType::k_i32, "");
static_assert(kField_dialog_reply_shown.type == ReplayType::k_ptr, "");

// The virtual clock (platform/replay_clock.h, shared with our engine's
// replay mode): kClockBase at the start, then 1000 / kFramesPerSecond ms per
// frame (CChitin::TIMER_UPDATES_PER_SECOND, 30 by default), exact on average
// (frame N reads kClockBase + N * 1000 / 30).
constexpr DWORD kClockBase = platform::replay_clock::kClockBase;
constexpr DWORD kFramesPerSecond = platform::replay_clock::kFramesPerSecond;

// The timer ids handed out for timeSetEvent.
constexpr UINT kFirstTimerId = 0x7100;

struct Timer {
    UINT id;
    UINT delay;
    LPTIMECALLBACK callback;
    DWORD_PTR user;
    UINT flags;
    DWORD due;
};

// Recorder state. g_frame and g_now are read on every thread.
std::atomic<uint32_t> g_frame { 0 };
// Frames with no AI update since the previous frame and none in progress
// (main thread; see OnFrame), and au_counter at the previous frame.
uint32_t g_stalledFrames = 0;
int32_t g_lastAu = 0;
std::atomic<DWORD> g_now { kClockBase };
std::atomic<DWORD> g_mainThread { 0 };
std::atomic<HWND> g_window { nullptr };
std::atomic<bool> g_wrongThreadLogged { false };
// Which calls of the frame hook are frames (main thread).
platform::replay_clock::FrameGate g_gate;
// The game window's own procedure, behind the focus guard (GuardProc).
std::atomic<WNDPROC> g_gameProc { nullptr };
bool g_focusGuard = true; // off only with IWD2R_TEST_NO_FOCUS_GUARD
uint32_t g_stealFocusFrame = 0; // IWD2R_TEST_STEAL_FOCUS
unsigned g_slowReadsMs = 0; // IWD2R_TEST_SLOW_READS
std::atomic<uint32_t> g_slowedReads { 0 };
std::string g_dir;
unsigned g_dumpEvery = 30;
unsigned g_seed = 1;
bool g_haveScript = false;
bool g_scriptFinished = false;
std::atomic<uint32_t> g_inputTiddata { 0 }; // _tiddata of the thread that polls input
std::atomic<int> g_lastX { 400 }; // the cursor in client pixels when the scenario does not say
std::atomic<int> g_lastY { 300 };
uint32_t g_frameTarget = 0; // where the patched jmp went: CChitin::SynchronousUpdate
// Main thread: the current call of the frame hook counted as a frame, so
// OnFrameEnd fires the timers when CChitin::SynchronousUpdate returns.
bool g_frameCounted = false;

std::mutex g_timerMutex;
std::vector<Timer> g_timers;
UINT g_nextTimerId = kFirstTimerId;

std::mutex g_messageMutex;
std::deque<MSG> g_messages; // synthesized, for the main thread

std::mutex g_logMutex;
std::FILE* g_log = nullptr;

// The real functions behind the hooked IAT slots.
using GetAsyncKeyStateFn = SHORT(WINAPI*)(int);
using GetCursorPosFn = BOOL(WINAPI*)(LPPOINT);
using SetCursorPosFn = BOOL(WINAPI*)(int, int);
using PeekMessageAFn = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
using GetMessageAFn = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT);
using TimeGetTimeFn = DWORD(WINAPI*)();
using GetTickCountFn = DWORD(WINAPI*)();
using TimeSetEventFn = MMRESULT(WINAPI*)(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT);
using TimeKillEventFn = MMRESULT(WINAPI*)(UINT);
using LoadLibraryAFn = HMODULE(WINAPI*)(LPCSTR);
using MessageBoxAFn = int(WINAPI*)(HWND, LPCSTR, LPCSTR, UINT);
using ReadFileFn = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);

PeekMessageAFn g_realPeekMessageA = nullptr;
GetMessageAFn g_realGetMessageA = nullptr;
TimeKillEventFn g_realTimeKillEvent = nullptr;
LoadLibraryAFn g_realLoadLibraryA = nullptr;
ReadFileFn g_realReadFile = nullptr;

void Log(const char* format, ...)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_log) {
        return;
    }
    std::fprintf(g_log, "[frame %u] ", g_frame.load());
    va_list args;
    va_start(args, format);
    std::vfprintf(g_log, format, args);
    va_end(args);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

std::string Env(const char* name)
{
    char buffer[4096];
    const DWORD n = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
    return n > 0 && n < sizeof(buffer) ? std::string(buffer, n) : std::string();
}

bool ParseUnsigned(const std::string& text, unsigned* value)
{
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const unsigned long v = std::strtoul(text.c_str(), &end, 10);
    if (*end != '\0') {
        return false;
    }
    *value = static_cast<unsigned>(v);
    return true;
}

std::string PathIn(const std::string& name)
{
    return g_dir + "\\" + name;
}

// Writes a file through a temporary name and a rename, so a reader never sees
// half of it.
bool WriteFileAtomically(const std::string& name, const std::string& text)
{
    const std::string path = PathIn(name);
    const std::string temp = path + ".tmp";
    HANDLE file = CreateFileA(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(file);
    return ok && written == text.size() && MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// ---------------------------------------------------------------------------
// Reading the original's memory

// Copies size bytes from address; false if any of them cannot be read.
bool SafeRead(uint32_t address, void* out, size_t size)
{
    if (address < 0x10000) {
        return false;
    }
    __try {
        std::memcpy(out, reinterpret_cast<const void*>(static_cast<uintptr_t>(address)), size);
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER
                                                                 : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

uint32_t ReadU32(uint32_t address, bool* ok)
{
    uint32_t value = 0;
    *ok = SafeRead(address, &value, 4) && *ok;
    return value;
}

int32_t ReadI32(uint32_t address, bool* ok)
{
    return static_cast<int32_t>(ReadU32(address, ok));
}

int16_t ReadI16(uint32_t address, bool* ok)
{
    int16_t value = 0;
    *ok = SafeRead(address, &value, 2) && *ok;
    return value;
}

uint8_t ReadU8(uint32_t address, bool* ok)
{
    uint8_t value = 0;
    *ok = SafeRead(address, &value, 1) && *ok;
    return value;
}

// A member of the object at base.
uint32_t At(uint32_t base, const ReplayField& field)
{
    return base + field.offset;
}

// The calling thread's _tiddata in the original's CRT (its _getptd).
uint32_t OriginalTiddata()
{
    using Getptd = void*(__cdecl*)();
    return static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(reinterpret_cast<Getptd>(static_cast<uintptr_t>(kVanillaGetptd))()));
}

// ---------------------------------------------------------------------------
// The state dump (oracle/replay/schema.json)

std::string JsonString(const char* text, size_t length)
{
    std::string out = "\"";
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x7F) {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            out += escaped;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string Number(long long value)
{
    return std::to_string(value);
}

// The object an id names (the create_char screen's sprite, a party member,
// an object on an area's list): CGameObjectArray::GetShare's THREAD_ASYNCH
// lookup, without the call. 0 when the id does not resolve.
uint32_t ObjectById(uint32_t chitin, int32_t id)
{
    bool ok = true;
    const uint32_t game = ReadU32(At(chitin, kField_object_game), &ok);
    const uint32_t array = ReadU32(At(game, kField_objects_array), &ok);
    const int16_t maxIndex = ReadI16(At(game, kField_objects_max_index), &ok);
    const int16_t nextId = ReadI16(At(game, kField_objects_next_id), &ok);
    const int16_t index = static_cast<int16_t>((id >> 16) & 0x7FFF);
    if (!ok || id == -1 || maxIndex < index || (id & 0x8000) != 0 || nextId <= static_cast<int16_t>(id)) {
        return 0;
    }
    const uint32_t entry = array + static_cast<uint32_t>(index) * kField_object_entry_size.offset;
    if (ReadI16(At(entry, kField_object_entry_id), &ok) != static_cast<int16_t>(id)) {
        return 0;
    }
    const uint32_t object = ReadU32(At(entry, kField_object_entry_ptr), &ok);
    return ok ? object : 0;
}

std::string ChargenJson(uint32_t chitin, uint32_t screen)
{
    bool ok = true;
    const int32_t spriteId = ReadI32(At(screen, kField_create_char_sprite), &ok);
    const int32_t skillPoints = ReadI32(At(screen, kField_create_char_skill_points), &ok);
    const ReplayField memory[] = { kField_create_char_memory_str, kField_create_char_memory_dex,
        kField_create_char_memory_con, kField_create_char_memory_int, kField_create_char_memory_wis,
        kField_create_char_memory_chr };
    std::string rolls = "[";
    for (const ReplayField& field : memory) {
        rolls += Number(ReadU8(At(screen, field), &ok)) + ", ";
    }
    rolls += Number(ReadI32(At(screen, kField_create_char_memory_extra), &ok)) + "]";
    if (!ok) {
        return "null";
    }
    std::string race = "null";
    std::string subrace = "null";
    std::string klass = "null";
    std::string abilities = "null";
    std::string feats = "null";
    std::string portrait = "null";
    const uint32_t sprite = ObjectById(chitin, spriteId);
    if (sprite) {
        bool spriteOk = true;
        race = Number(ReadU8(At(sprite, kField_sprite_race), &spriteOk));
        subrace = Number(ReadU8(At(sprite, kField_sprite_subrace), &spriteOk));
        klass = Number(ReadU8(At(sprite, kField_sprite_class), &spriteOk));
        const ReplayField stats[] = { kField_sprite_str, kField_sprite_dex, kField_sprite_con, kField_sprite_int,
            kField_sprite_wis, kField_sprite_chr };
        abilities = "[";
        for (const ReplayField& field : stats) {
            abilities += (abilities.size() > 1 ? ", " : "") + Number(ReadU8(At(sprite, field), &spriteOk));
        }
        abilities += "]";
        feats = "[";
        for (uint32_t i = 0; i < kField_sprite_feats.count; ++i) {
            feats += (i ? ", " : "") + Number(ReadU32(At(sprite, kField_sprite_feats) + 4 * i, &spriteOk));
        }
        feats += "]";
        char resref[8] = {};
        spriteOk = SafeRead(At(sprite, kField_sprite_portrait), resref, sizeof(resref)) && spriteOk;
        portrait = JsonString(resref, strnlen(resref, sizeof(resref)));
        if (!spriteOk) {
            race = subrace = klass = abilities = feats = portrait = "null";
        }
    }
    return "{\"race\": " + race + ", \"subrace\": " + subrace + ", \"class\": " + klass + ", \"abilities\": "
        + abilities + ", \"rolls\": " + rolls + ", \"skill_points\": " + Number(skillPoints) + ", \"feats\": " + feats
        + ", \"portrait\": " + portrait + "}";
}

// One party member: its values are null when the id does not resolve or a
// read fails.
std::string PartyMemberJson(uint32_t chitin, int32_t id)
{
    std::string values = "\"x\": null, \"y\": null, \"hp\": null, \"max_hp\": null, \"skills\": null, "
                         "\"action\": null, \"modal\": null, \"facing\": null";
    const uint32_t sprite = ObjectById(chitin, id);
    if (sprite) {
        bool ok = true;
        const int32_t x = ReadI32(At(sprite, kField_sprite_pos), &ok);
        const int32_t y = ReadI32(At(sprite, kField_sprite_pos) + 4, &ok);
        const int16_t hp = ReadI16(At(sprite, kField_sprite_hp), &ok);
        const int16_t maxHp = ReadI16(At(sprite, kField_sprite_max_hp), &ok);
        std::string skills = "[";
        for (uint32_t i = 0; i < kField_sprite_skills.count; ++i) {
            skills += (i ? ", " : "") + Number(static_cast<int8_t>(ReadU8(At(sprite, kField_sprite_skills) + i, &ok)));
        }
        skills += "]";
        const int16_t action = ReadI16(At(sprite, kField_sprite_action), &ok);
        const uint8_t modal = ReadU8(At(sprite, kField_sprite_modal), &ok);
        const int16_t facing = ReadI16(At(sprite, kField_sprite_facing), &ok);
        if (ok) {
            values = "\"x\": " + Number(x) + ", \"y\": " + Number(y) + ", \"hp\": " + Number(hp)
                + ", \"max_hp\": " + Number(maxHp) + ", \"skills\": " + skills + ", \"action\": " + Number(action)
                + ", \"modal\": " + Number(modal) + ", \"facing\": " + Number(facing);
        }
    }
    return "{\"id\": " + Number(id) + ", " + values + "}";
}

// CGameObject::TYPE_DOOR and TYPE_SPRITE, the m_objectType bytes the
// original's constructors store (docs/evidence/485AC0.md).
constexpr uint8_t kTypeDoor = 0x21;
constexpr uint8_t kTypeSprite = 0x31;
// A bound on a list walk, against a list caught mid-change.
constexpr int kMaxListNodes = 100000;

// Calls visit(object) for each object of type on the CPtrList whose head
// node pointer is at headAddress, head to tail (MFC's CNode: pNext, data =
// the object id).
template <typename Visit>
void ObjectsOn(uint32_t chitin, uint32_t headAddress, uint8_t type, Visit visit)
{
    bool ok = true;
    uint32_t node = ReadU32(headAddress, &ok);
    for (int n = 0; ok && node && n < kMaxListNodes; ++n) {
        const int32_t id = ReadI32(At(node, kField_list_node_data), &ok);
        node = ReadU32(At(node, kField_list_node_next), &ok);
        if (!ok) {
            break;
        }
        const uint32_t object = ObjectById(chitin, id);
        bool typeOk = true;
        if (object && ReadU8(At(object, kField_object_type), &typeOk) == type && typeOk) {
            visit(object);
        }
    }
}

// The world block's "doors" (the JSON array), "explored_count" and
// "creatures".
struct AreaValues {
    std::string doors;
    long long explored = 0;
    long long creatures = 0;
};

// The area's doors (its front list's order), explored tiles and sprites;
// none for no area.
AreaValues ReadArea(uint32_t chitin, uint32_t area)
{
    std::string doors = "[";
    long long explored = 0;
    long long creatures = 0;
    if (area) {
        ObjectsOn(chitin, At(area, kField_area_vert_sort), kTypeDoor, [&](uint32_t door) {
            char name[32] = {};
            bool ok = SafeRead(At(door, kField_door_name), name, sizeof(name));
            const uint32_t flags = ReadU32(At(door, kField_door_flags), &ok);
            if (ok) {
                doors += (doors.size() > 1 ? ", " : "") + std::string("{\"name\": ")
                    + JsonString(name, strnlen(name, sizeof(name))) + ", \"open\": " + Number(flags & 1u)
                    + ", \"locked\": " + Number((flags >> 1) & 1u) + "}";
            }
        });
        bool ok = true;
        const uint32_t map = ReadU32(At(area, kField_area_visibility_map), &ok);
        const int32_t size = ReadI32(At(area, kField_area_visibility_size), &ok);
        if (ok && map && size > 0) {
            std::vector<uint8_t> tiles(static_cast<size_t>(size));
            if (SafeRead(map, tiles.data(), tiles.size())) {
                for (const uint8_t tile : tiles) {
                    explored += (tile & 0x80) != 0; // CVisibilityMap::EXPLORED_FLAG
                }
            }
        }
        const ReplayField lists[] = { kField_area_vert_sort, kField_area_vert_sort_back, kField_area_vert_sort_flight };
        for (const ReplayField& list : lists) {
            ObjectsOn(chitin, At(area, list), kTypeSprite, [&](uint32_t) { ++creatures; });
        }
    }
    doors += "]";
    AreaValues values;
    values.doors = doors;
    values.explored = explored;
    values.creatures = creatures;
    return values;
}

// CScreenWorld::m_internalLoadedDialog: the "dialog" member.
std::string DialogJson(uint32_t world)
{
    bool ok = true;
    const uint8_t active = world ? ReadU8(At(world, kField_dialog_active), &ok) : 0;
    if (!world || !ok || !active) {
        return "{\"active\": 0, \"speaker\": -1, \"entry\": -1, \"responses\": 0}";
    }
    const int32_t talker = ReadI32(At(world, kField_dialog_talker), &ok);
    const uint32_t entryIndex = ReadU32(At(world, kField_dialog_entry), &ok);
    const int32_t waiting = ReadI32(At(world, kField_dialog_waiting), &ok);
    const uint32_t entries = ReadU32(At(world, kField_dialog_entries), &ok);
    const int32_t entriesSize = ReadI32(At(world, kField_dialog_entries_size), &ok);
    long long responses = 0;
    if (ok && waiting && entries && entriesSize > 0 && entryIndex < static_cast<uint32_t>(entriesSize)) {
        bool entryOk = true;
        const uint32_t entry = ReadU32(entries + 4u * entryIndex, &entryOk);
        const uint32_t replies = entry ? ReadU32(At(entry, kField_dialog_replies), &entryOk) : 0;
        const int32_t count = entry ? ReadI32(At(entry, kField_dialog_replies_size), &entryOk) : 0;
        for (int32_t i = 0; entryOk && replies && i < count && i < kMaxListNodes; ++i) {
            const uint32_t reply = ReadU32(replies + 4u * static_cast<uint32_t>(i), &entryOk);
            bool replyOk = true;
            if (reply && ReadU32(At(reply, kField_dialog_reply_shown), &replyOk) != 0 && replyOk) {
                ++responses;
            }
        }
    }
    return "{\"active\": 1, \"speaker\": " + Number(talker) + ", \"entry\": " + Number(entryIndex)
        + ", \"responses\": " + Number(responses) + "}";
}

// The area on view and the party (CInfGame::GetVisibleArea and
// GetCharacterId read without the calls), the area's doors, explored tiles
// and sprites, and the dialog of world, the CScreenWorld. null when CInfGame
// cannot be read.
std::string WorldJson(uint32_t chitin, uint32_t world)
{
    bool ok = true;
    const uint32_t game = ReadU32(At(chitin, kField_object_game), &ok);
    const uint8_t visible = ReadU8(At(game, kField_visible_area), &ok);
    int16_t count = ReadI16(At(game, kField_characters_count), &ok);
    if (!ok) {
        return "null";
    }
    std::string area = "\"\"";
    uint32_t pArea = 0;
    if (visible < kField_game_areas.count) {
        bool areaOk = true;
        pArea = ReadU32(At(game, kField_game_areas) + 4u * visible, &areaOk);
        char resref[8] = {};
        if (!areaOk) {
            pArea = 0;
        } else if (pArea && SafeRead(At(pArea, kField_area_resref), resref, sizeof(resref))) {
            area = JsonString(resref, strnlen(resref, sizeof(resref)));
        }
    }
    if (count < 0) {
        count = 0;
    }
    if (count > static_cast<int16_t>(kField_character_portraits.count)) {
        count = static_cast<int16_t>(kField_character_portraits.count);
    }
    std::string party = "[";
    for (int16_t i = 0; i < count; ++i) {
        const int32_t id = ReadI32(At(game, kField_character_portraits) + 4u * i, &ok);
        party += (i ? ", " : "") + PartyMemberJson(chitin, id);
    }
    party += "]";
    if (!ok) {
        return "null";
    }
    const AreaValues values = ReadArea(chitin, pArea);
    return "{\"area\": " + area + ", \"party\": " + party + ", \"doors\": " + values.doors + ", \"dialog\": "
        + DialogJson(world) + ", \"explored_count\": " + Number(values.explored)
        + ", \"creatures\": " + Number(values.creatures) + "}";
}

// The dump of the current frame; checkpoint is null for a periodic dump.
std::string DumpJson(const char* checkpoint)
{
    bool ok = true;
    const uint32_t chitin = ReadU32(kField_baldur_chitin.address, &ok);
    const uint32_t active = chitin ? ReadU32(At(chitin, kField_active_engine), &ok) : 0;
    const char* screen = active ? "unknown" : "none";
    for (const ReplayScreen& s : kReplayScreens) {
        if (active && ReadU32(At(chitin, s.field), &ok) == active) {
            screen = s.name;
        }
    }
    const uint32_t tiddata = OriginalTiddata();
    const uint32_t rng = ReadU32(tiddata + kVanillaTiddataRandStart, &ok);
    const uint32_t inputTiddata = g_inputTiddata.load();
    bool aiOk = true;
    const uint32_t aiRng = inputTiddata ? ReadU32(inputTiddata + kVanillaTiddataRandStart, &aiOk) : 0;
    const int32_t auCounter = chitin ? ReadI32(At(chitin, kField_au_counter), &ok) : 0;
    const uint8_t fullscreen = chitin ? ReadU8(At(chitin, kField_fullscreen), &ok) : 0;
    const int32_t inAsync = chitin ? ReadI32(At(chitin, kField_in_async_update), &ok) : 0;
    const uint8_t reinitializing = chitin ? ReadU8(At(chitin, kField_reinitializing), &ok) : 0;
    const uint8_t loading = chitin ? ReadU8(At(chitin, kField_progress_bar_active), &ok) : 0;
    if (!ok) {
        Log("dump: some fields could not be read (CBaldurChitin at 0x%08X)", chitin);
    }
    std::string json = "{\n  \"frame\": " + Number(g_frame.load()) + ",\n";
    if (checkpoint) {
        json += "  \"checkpoint\": " + JsonString(checkpoint, std::strlen(checkpoint)) + ",\n";
    }
    json += "  \"screen\": \"" + std::string(screen) + "\",\n";
    json += "  \"rng_state\": " + Number(rng) + ",\n";
    json += "  \"globals\": {\"au_counter\": " + Number(auCounter) + ", \"fullscreen\": " + Number(fullscreen)
        + ", \"in_async_update\": " + Number(inAsync) + ", \"ai_stalled_frames\": " + Number(g_stalledFrames)
        + ", \"reinitializing\": " + Number(reinitializing) + ", \"loading\": " + Number(loading)
        + ", \"loads\": " + Number(g_gate.Loads()) + ", \"held_frames\": " + Number(g_gate.HeldFrames())
        + ", \"ai_rng_state\": "
        + (inputTiddata && aiOk ? Number(aiRng) : std::string("null")) + "},\n";
    const bool chargen = std::strcmp(screen, "create_char") == 0;
    json += "  \"chargen\": " + (chargen ? ChargenJson(chitin, active) : std::string("null")) + ",\n";
    const bool world = std::strcmp(screen, "world") == 0;
    json += "  \"world\": " + (world ? WorldJson(chitin, active) : std::string("null")) + "\n}\n";
    return json;
}

void WriteDump(const char* checkpoint)
{
    const std::string name = checkpoint ? "checkpoint-" + std::string(checkpoint) + ".json"
                                        : "frame-" + std::to_string(g_frame.load()) + ".json";
    if (!WriteFileAtomically(name, DumpJson(checkpoint))) {
        Log("cannot write %s: error %lu", name.c_str(), GetLastError());
    } else if (checkpoint) {
        Log("wrote %s", name.c_str());
    }
}

void WriteStatus()
{
    const char* state = !g_haveScript ? "no-script" : g_scriptFinished ? "finished"
                                                                       : "running";
    char text[160];
    std::snprintf(text, sizeof(text), "frame=%u\nscript_line=%d\nstate=%s\n", g_frame.load(),
        g_haveScript ? recorder::ScriptLine() : 0, state);
    WriteFileAtomically("status.txt", text);
}

// ---------------------------------------------------------------------------
// The frame

// Writes the game window's client area as <name>.bmp (24-bit), copied from
// the window's DC on the main thread, right before the frame is drawn: the
// last frame presented. (PrintWindow with PW_RENDERFULLCONTENT returns black
// for the original's DirectDraw window.)
bool WriteWindowShot(const std::string& name, std::string* why)
{
    RECT rect;
    const HWND window = g_window.load();
    if (!window || !GetClientRect(window, &rect) || rect.right <= 0 || rect.bottom <= 0) {
        *why = "no game window";
        return false;
    }
    const int width = rect.right;
    const int height = rect.bottom;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height; // bottom-up, as a .bmp stores it
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!memory || !bitmap) {
        *why = "cannot create a bitmap";
        if (memory) {
            DeleteDC(memory);
        }
        return false;
    }
    HGDIOBJ old = SelectObject(memory, bitmap);
    HDC windowDc = GetDC(window);
    const BOOL printed = BitBlt(memory, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);
    ReleaseDC(window, windowDc);
    GdiFlush();
    const DWORD stride = (static_cast<DWORD>(width) * 3 + 3) & ~3u;
    const DWORD imageSize = stride * static_cast<DWORD>(height);
    BITMAPFILEHEADER header = {};
    header.bfType = 0x4D42; // "BM"
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + imageSize;
    std::string text(reinterpret_cast<const char*>(&header), sizeof(header));
    text.append(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
    text.append(static_cast<const char*>(bits), imageSize);
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    if (!printed) {
        *why = "BitBlt failed";
        return false;
    }
    if (!WriteFileAtomically(name + ".bmp", text)) {
        *why = "cannot write " + name + ".bmp";
        return false;
    }
    *why = std::to_string(width) + "x" + std::to_string(height);
    return true;
}

class Events : public recorder::ScriptEvents {
public:
    void Shot(const std::string& name, int line) override
    {
        std::string why;
        if (WriteWindowShot(name, &why)) {
            Log("line %d: shot %s.bmp (%s)", line, name.c_str(), why.c_str());
        } else {
            Log("line %d: shot %s: %s", line, name.c_str(), why.c_str());
        }
    }

    void Quit(int line) override
    {
        MSG msg = {};
        msg.hwnd = g_window.load();
        msg.message = WM_CLOSE;
        msg.time = g_now.load();
        msg.pt.x = g_lastX.load();
        msg.pt.y = g_lastY.load();
        std::lock_guard<std::mutex> lock(g_messageMutex);
        g_messages.push_back(msg);
        Log("line %d: quit: WM_CLOSE queued for window %p", line, static_cast<void*>(msg.hwnd));
    }

    void Checkpoint(const std::string& name, int line) override
    {
        Log("line %d: checkpoint %s", line, name.c_str());
        WriteDump(name.c_str());
    }
};

// The focus guard. Activation messages are sent, not posted, so the message
// hooks never see them: they go straight to the window procedure. There,
// WM_ACTIVATEAPP(FALSE) calls OnAltTab(FALSE), which sets m_bReInitializing,
// and AsynchronousUpdate stops counting and polling input while frames go
// on; WM_ACTIVATEAPP(TRUE) calls OnAltTab(TRUE), which re-activates the
// engine. Either would make a recording depend on where the user's focus
// was. The guard swallows every WM_ACTIVATEAPP, WM_ACTIVATE(WA_INACTIVE) and
// WM_KILLFOCUS, from the window's creation on (it replaces the procedure in
// the window class's registration, HookRegisterClassExA), so the game
// behaves as active throughout, as our engine does headless (no focus
// events).
LRESULT CALLBACK GuardProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    const bool focus = message == WM_ACTIVATEAPP || message == WM_KILLFOCUS
        || (message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE);
    if (focus) {
        const char* name = message == WM_ACTIVATEAPP ? "WM_ACTIVATEAPP"
            : message == WM_KILLFOCUS                ? "WM_KILLFOCUS"
                                                     : "WM_ACTIVATE";
        if (g_focusGuard) {
            Log("focus guard: swallowed %s(%u)", name, static_cast<unsigned>(LOWORD(wParam)));
            return 0;
        }
        Log("focus guard off: %s(%u) passed to the game", name, static_cast<unsigned>(LOWORD(wParam)));
    }
    const WNDPROC game = g_gameProc.load();
    return game ? CallWindowProcA(game, window, message, wParam, lParam)
                : DefWindowProcA(window, message, wParam, lParam);
}

BOOL CALLBACK FindGameWindow(HWND window, LPARAM)
{
    RECT rect;
    if (IsWindowVisible(window) && GetClientRect(window, &rect) && rect.right > 0 && rect.bottom > 0) {
        g_window = window;
        return FALSE;
    }
    return TRUE;
}

// Makes window the foreground window, attaching to the current foreground
// window's input queue first (SetForegroundWindow alone is refused to a
// process that is not in the foreground). Test switches only.
void ForceForeground(HWND window)
{
    const HWND current = GetForegroundWindow();
    const DWORD currentThread = current ? GetWindowThreadProcessId(current, nullptr) : 0;
    const DWORD self = GetCurrentThreadId();
    const bool attach = currentThread && currentThread != self && AttachThreadInput(self, currentThread, TRUE);
    BringWindowToTop(window);
    SetForegroundWindow(window);
    SetFocus(window);
    if (attach) {
        AttachThreadInput(self, currentThread, FALSE);
    }
}

LRESULT CALLBACK ThiefProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(window, message, wParam, lParam);
}

// IWD2R_TEST_STEAL_FOCUS: a window of the recorder's own, on a thread of its
// own, takes the foreground from the game, as another application would.
DWORD WINAPI FocusThief(LPVOID)
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = ThiefProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "iwd2rFocusThief";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassA(&wc);
    HWND thief = CreateWindowExA(0, wc.lpszClassName, "iwd2r focus thief (recorder test)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 320, 120, nullptr, nullptr, wc.hInstance, nullptr);
    ForceForeground(thief);
    Log("focus thief %p created; the foreground window is now %p (game %p)", static_cast<void*>(thief),
        static_cast<void*>(GetForegroundWindow()), static_cast<void*>(g_window.load()));
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        DispatchMessageA(&msg);
    }
    return 0;
}

void FireTimers()
{
    std::vector<Timer> due;
    {
        std::lock_guard<std::mutex> lock(g_timerMutex);
        const DWORD now = g_now.load();
        for (size_t i = 0; i < g_timers.size();) {
            Timer& t = g_timers[i];
            if (static_cast<LONG>(now - t.due) < 0) {
                ++i;
                continue;
            }
            due.push_back(t);
            if (t.flags & TIME_PERIODIC) {
                t.due = now + t.delay; // at most once per frame
                ++i;
            } else {
                g_timers.erase(g_timers.begin() + static_cast<ptrdiff_t>(i));
            }
        }
    }
    for (const Timer& t : due) {
        if (t.flags & TIME_CALLBACK_EVENT_SET) {
            SetEvent(reinterpret_cast<HANDLE>(t.callback));
        } else if (t.flags & TIME_CALLBACK_EVENT_PULSE) {
            PulseEvent(reinterpret_cast<HANDLE>(t.callback));
        } else {
            t.callback(t.id, 0, t.user, 0, 0);
        }
    }
}

// The frame gate's question for this call: false when it is held.
bool IsFrame()
{
    if (g_frame.load() == 0) {
        return true; // the first call sets the recorder up
    }
    bool ok = true;
    const uint32_t chitin = ReadU32(kField_baldur_chitin.address, &ok);
    if (!ok || !chitin) {
        return true;
    }
    // In this order: see FrameGate::Counts.
    const int32_t au = ReadI32(At(chitin, kField_au_counter), &ok);
    const int32_t inAsync = ReadI32(At(chitin, kField_in_async_update), &ok);
    const uint8_t progress = ReadU8(At(chitin, kField_progress_bar_active), &ok);
    if (!ok) {
        return true;
    }
    const uint32_t loadsBefore = g_gate.Loads();
    if (!g_gate.Counts(au, inAsync != 0, progress != 0)) {
        if (g_gate.Stretch() == 1) {
            Log("frames held: drawn inside AI update %d (progress bar %s)", au, progress ? "up" : "down");
        } else if (g_gate.Stretch() == 0) {
            Log("frame held: the redraw the end of AI update %d asked for, after a frame already counted for it", au);
        }
        if (g_gate.Loads() != loadsBefore) {
            Log("load %u: AI update %d", g_gate.Loads(), au);
        }
        if (g_slowReadsMs) {
            Log("test: held call %u of AI update %d, %u reads slowed so far", g_gate.Stretch(), au,
                g_slowedReads.load());
        }
        return false;
    }
    if (g_gate.EndedStretch()) {
        Log("%u calls held inside the last AI update; frames go on (au_counter %d)", g_gate.EndedStretch(), au);
        if (g_slowReadsMs) {
            Log("test: %u reads slowed so far", g_slowedReads.load());
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// IWD2R_RAND_TRACE (docs/rand-trace.md): every rand() call as a line of
// rand.txt, "frame thread caller" (the caller's return address, hex), written
// by the frame hook. rand begins with a 5-byte call to _getptd; that call
// becomes a jmp to RandTrampoline, which logs and then makes the call itself.

CRITICAL_SECTION g_randLock;
std::vector<uint32_t> g_randLog;
bool g_randTrace = false;
uint32_t g_randGetptd = kVanillaGetptd;
uint32_t g_randContinue = kVanillaRand + 5;

extern "C" void __cdecl LogRand(uint32_t caller)
{
    EnterCriticalSection(&g_randLock);
    g_randLog.push_back(g_frame.load());
    g_randLog.push_back(GetCurrentThreadId());
    g_randLog.push_back(caller);
    LeaveCriticalSection(&g_randLock);
}

extern "C" __declspec(naked) void RandTrampoline()
{
    __asm {
        pushad
        pushfd
        push dword ptr [esp + 36]
        call LogRand
        add esp, 4
        popfd
        popad
        call dword ptr [g_randGetptd]
        jmp dword ptr [g_randContinue]
    }
}

void FlushRandTrace()
{
    if (!g_randTrace) {
        return;
    }
    std::vector<uint32_t> calls;
    EnterCriticalSection(&g_randLock);
    calls.swap(g_randLog);
    LeaveCriticalSection(&g_randLock);
    if (calls.empty()) {
        return;
    }
    std::FILE* out = std::fopen(PathIn("rand.txt").c_str(), "a");
    if (!out) {
        return;
    }
    for (size_t i = 0; i + 2 < calls.size(); i += 3) {
        std::fprintf(out, "%u %u %08x\n", calls[i], calls[i + 1], calls[i + 2]);
    }
    std::fclose(out);
}

extern "C" void __cdecl OnFrame()
{
    if (!IsFrame()) {
        return;
    }
    FlushRandTrace();
    const uint32_t frame = ++g_frame;
    if (frame == 1) {
        g_mainThread = GetCurrentThreadId();
        reinterpret_cast<void(__cdecl*)(unsigned int)>(static_cast<uintptr_t>(kVanillaSrand))(g_seed);
        Log("first frame on thread %lu; srand(%u) on it", g_mainThread.load(), g_seed);
    } else if (GetCurrentThreadId() != g_mainThread.load() && !g_wrongThreadLogged.exchange(true)) {
        // The dumps and the scenario assume the main thread (the AI thread
        // waits only for it); say so once if that ever breaks.
        Log("WARNING: the frame hook ran on thread %lu, not the main thread %lu", GetCurrentThreadId(),
            g_mainThread.load());
    }
    if (!g_window.load()) {
        EnumThreadWindows(GetCurrentThreadId(), FindGameWindow, 0);
    }
    if (frame == 1 && g_window.load()) {
        RECT rect;
        GetClientRect(g_window.load(), &rect);
        Log("game window %p, client %ldx%ld", static_cast<void*>(g_window.load()), rect.right, rect.bottom);
        if (g_stealFocusFrame) {
            // The test starts from a game that has the focus, so the thief
            // has something to take.
            ForceForeground(g_window.load());
            Log("test: game window made foreground (foreground %p)", static_cast<void*>(GetForegroundWindow()));
        }
    }
    if (g_stealFocusFrame && frame == g_stealFocusFrame) {
        Log("test: stealing the focus");
        CloseHandle(CreateThread(nullptr, 0, FocusThief, nullptr, 0, nullptr));
    }
    if (frame == 1 && g_haveScript) {
        int width = 800;
        int height = 600;
        RECT rect;
        if (g_window.load() && GetClientRect(g_window.load(), &rect)) {
            width = rect.right;
            height = rect.bottom;
        }
        recorder::StartScript(width, height);
        g_lastX = width / 2;
        g_lastY = height / 2;
    }
    if (g_haveScript && !g_scriptFinished) {
        Events events;
        if (recorder::AdvanceScript(events) || !recorder::ScriptRunning()) {
            g_scriptFinished = true;
            Log("scenario finished");
        }
    }
    {
        // Every frame must have had an AI update since the previous frame
        // (the gate holds the calls drawn from inside one, as a load
        // redrawing its progress from inside AsynchronousUpdate). A frame
        // without one is a stall: the AI thread ran without counting
        // (m_bReInitializing, a focus loss) or did not run.
        bool ok = true;
        const uint32_t chitin = ReadU32(kField_baldur_chitin.address, &ok);
        if (chitin) {
            const int32_t au = ReadI32(At(chitin, kField_au_counter), &ok);
            const int32_t inAsync = ReadI32(At(chitin, kField_in_async_update), &ok);
            if (ok && au == g_lastAu && inAsync == 0) {
                if (g_stalledFrames++ == 0) {
                    Log("AI stall: no AI update since the previous frame (au_counter %d)", au);
                }
            }
            g_lastAu = au;
        }
    }
    if (g_dumpEvery && frame % g_dumpEvery == 0) {
        WriteDump(nullptr);
    }
    WriteStatus();
    g_now = platform::replay_clock::TickAfterFrame(frame);
    g_frameCounted = true;
}

// After CChitin::SynchronousUpdate returns: a call that counted as a frame
// fires the due timers now, so the AI update they wake runs after the
// frame's render (platform/replay_clock.h, "The order inside a frame"). A
// held call fires nothing, as before.
extern "C" void __cdecl OnFrameEnd()
{
    if (!g_frameCounted) {
        return;
    }
    g_frameCounted = false;
    FireTimers();
}

// Replaces the first instruction of CBaldurChitin::SynchronousUpdate. It runs
// OnFrame with every register and flag kept, then calls the original jmp's
// target, CChitin::SynchronousUpdate (thiscall, no arguments, a plain ret
// at 0x790CC3, 0x790D87 and 0x790DDE), with `this` (ecx) untouched, then
// OnFrameEnd, and returns to WinMain's call at 0x79284B. The extra return
// address on the stack is the only difference SynchronousUpdate sees.
extern "C" __declspec(naked) void FrameTrampoline()
{
    __asm {
        pushad
        pushfd
        call OnFrame
        popfd
        popad
        call dword ptr [g_frameTarget]
        pushad
        pushfd
        call OnFrameEnd
        popfd
        popad
        ret
    }
}

// ---------------------------------------------------------------------------
// Hooks

void NoteInputThread()
{
    const DWORD mainThread = g_mainThread.load();
    if (mainThread != 0 && GetCurrentThreadId() != mainThread && g_inputTiddata.load() == 0) {
        g_inputTiddata = OriginalTiddata();
        Log("input polled on thread %lu; its _tiddata is 0x%08X", GetCurrentThreadId(), g_inputTiddata.load());
    }
}

SHORT WINAPI HookGetAsyncKeyState(int vk)
{
    NoteInputThread();
    short state = 0;
    return recorder::ScriptKey(vk, &state) ? state : 0;
}

BOOL WINAPI HookGetCursorPos(LPPOINT point)
{
    if (!point) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    NoteInputThread();
    int x = 0;
    int y = 0;
    if (recorder::ScriptCursor(&x, &y)) {
        g_lastX = x;
        g_lastY = y;
    }
    point->x = g_lastX.load();
    point->y = g_lastY.load();
    if (const HWND window = g_window.load()) {
        ClientToScreen(window, point);
    }
    return TRUE;
}

BOOL WINAPI HookSetCursorPos(int x, int y)
{
    // The real cursor stays where the user has it; the game's own moves
    // become the scenario cursor until the scenario sets one.
    POINT point = { x, y };
    if (const HWND window = g_window.load()) {
        ScreenToClient(window, &point);
    }
    g_lastX = point.x;
    g_lastY = point.y;
    return TRUE;
}

bool IsUserInput(UINT message)
{
    return (message >= WM_KEYFIRST && message <= WM_KEYLAST) || (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
        || message == WM_INPUT;
}

// A synthesized message for this call, when the main thread asks and one
// matches the filter.
bool TakeSynthesized(LPMSG msg, HWND window, UINT first, UINT last, bool remove)
{
    if (GetCurrentThreadId() != g_mainThread.load()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_messageMutex);
    for (auto it = g_messages.begin(); it != g_messages.end(); ++it) {
        const bool windowMatches = !window || window == it->hwnd;
        const bool rangeMatches = (first == 0 && last == 0) || (it->message >= first && it->message <= last);
        if (windowMatches && rangeMatches) {
            *msg = *it;
            if (remove) {
                g_messages.erase(it);
            }
            return true;
        }
    }
    return false;
}

BOOL WINAPI HookPeekMessageA(LPMSG msg, HWND window, UINT first, UINT last, UINT flags)
{
    if (TakeSynthesized(msg, window, first, last, (flags & PM_REMOVE) != 0)) {
        return TRUE;
    }
    for (;;) {
        const BOOL found = g_realPeekMessageA(msg, window, first, last, flags);
        if (!found || !IsUserInput(msg->message)) {
            return found;
        }
        if (!(flags & PM_REMOVE)) {
            MSG dropped;
            g_realPeekMessageA(&dropped, msg->hwnd, msg->message, msg->message, PM_REMOVE);
        }
    }
}

BOOL WINAPI HookGetMessageA(LPMSG msg, HWND window, UINT first, UINT last)
{
    if (TakeSynthesized(msg, window, first, last, true)) {
        return TRUE;
    }
    for (;;) {
        const BOOL got = g_realGetMessageA(msg, window, first, last);
        if (got <= 0 || !IsUserInput(msg->message)) {
            return got;
        }
        // Dropped. Rather than block for the next message, hand back a
        // WM_NULL when nothing else is queued.
        MSG next;
        if (!g_realPeekMessageA(&next, window, first, last, PM_NOREMOVE)) {
            msg->message = WM_NULL;
            msg->wParam = 0;
            msg->lParam = 0;
            return TRUE;
        }
    }
}

DWORD WINAPI HookTime()
{
    return g_now.load();
}

// The wall clock: kWallClockYear-Month-Day (platform/replay_clock.h) plus
// the virtual clock's milliseconds, the same as local and as UTC time
// (GetTimeZoneInformation answers UTC), so time() reads
// replay_clock::WallClockSeconds, as in our engine's replay mode. The
// original seeds the AI thread's rand with srand(time(NULL)) when the main
// menu starts (CScreenConnection::TimerAsynchronousUpdate, 0x5FB3E0), and
// VC6's time() reads GetLocalTime and the time zone.
void VirtualWallClock(LPSYSTEMTIME out)
{
    SYSTEMTIME start = {};
    start.wYear = platform::replay_clock::kWallClockYear;
    start.wMonth = platform::replay_clock::kWallClockMonth;
    start.wDay = platform::replay_clock::kWallClockDay; // Icewind Dale II's release
    FILETIME file;
    SystemTimeToFileTime(&start, &file);
    ULARGE_INTEGER t;
    t.LowPart = file.dwLowDateTime;
    t.HighPart = file.dwHighDateTime;
    t.QuadPart += static_cast<ULONGLONG>(g_now.load() - kClockBase) * 10000;
    file.dwLowDateTime = t.LowPart;
    file.dwHighDateTime = t.HighPart;
    FileTimeToSystemTime(&file, out);
}

void WINAPI HookGetLocalTime(LPSYSTEMTIME out)
{
    VirtualWallClock(out);
}

void WINAPI HookGetSystemTime(LPSYSTEMTIME out)
{
    VirtualWallClock(out);
}

DWORD WINAPI HookGetTimeZoneInformation(LPTIME_ZONE_INFORMATION out)
{
    std::memset(out, 0, sizeof(*out));
    return TIME_ZONE_ID_UNKNOWN;
}

MMRESULT WINAPI HookTimeSetEvent(UINT delay, UINT, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags)
{
    std::lock_guard<std::mutex> lock(g_timerMutex);
    Timer t = { g_nextTimerId++, delay, callback, user, flags, g_now.load() + delay };
    g_timers.push_back(t);
    Log("timeSetEvent(%u ms, %s) = %u, driven by the virtual clock", delay,
        (flags & TIME_PERIODIC) ? "periodic" : "one shot", t.id);
    return t.id;
}

MMRESULT WINAPI HookTimeKillEvent(UINT id)
{
    {
        std::lock_guard<std::mutex> lock(g_timerMutex);
        for (auto it = g_timers.begin(); it != g_timers.end(); ++it) {
            if (it->id == id) {
                g_timers.erase(it);
                Log("timeKillEvent(%u)", id);
                return TIMERR_NOERROR;
            }
        }
    }
    return g_realTimeKillEvent ? g_realTimeKillEvent(id) : MMSYSERR_INVALPARAM;
}

HMODULE WINAPI HookLoadLibraryA(LPCSTR name)
{
    if (name) {
        const char* base = name;
        for (const char* p = name; *p; ++p) {
            if (*p == '\\' || *p == '/') {
                base = p + 1;
            }
        }
        if (_stricmp(base, "binkw32") == 0 || _stricmp(base, "binkw32.dll") == 0) {
            Log("LoadLibraryA(%s) refused: no movies, as in our engine", name);
            SetLastError(ERROR_MOD_NOT_FOUND);
            return nullptr;
        }
    }
    return g_realLoadLibraryA(name);
}

// IWD2R_TEST_SLOW_READS: a slow disk, from the first frame on, for every
// thread but the main one (the AI thread loads the areas), to show that a
// load's timing does not reach the dumps.
BOOL WINAPI HookReadFile(HANDLE file, LPVOID buffer, DWORD size, LPDWORD read, LPOVERLAPPED overlapped)
{
    const DWORD mainThread = g_mainThread.load();
    if (g_slowReadsMs && mainThread != 0 && GetCurrentThreadId() != mainThread) {
        ++g_slowedReads;
        Sleep(g_slowReadsMs);
    }
    return g_realReadFile(file, buffer, size, read, overlapped);
}

int WINAPI HookMessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
    // Our engine's headless answer (platform/window.cpp): the first button.
    int answer = IDOK;
    switch (type & 0x0Fu) {
    case MB_YESNO:
    case MB_YESNOCANCEL:
        answer = IDYES;
        break;
    default:
        break;
    }
    Log("MessageBoxA(\"%s\", \"%s\", 0x%X) answered %d without showing it", text ? text : "",
        caption ? caption : "", type, answer);
    return answer;
}

// The system input settings: our engine's fixed values
// (platform/input_settings.h, shared with platform/window.cpp), so double
// clicks and key repeats take the same frames on every machine. Anything
// else goes to the real function.
UINT WINAPI HookGetDoubleClickTime()
{
    return platform::input_settings::kDoubleClickTime;
}

using GetSystemMetricsFn = int(WINAPI*)(int);
GetSystemMetricsFn g_realGetSystemMetrics = nullptr;

int WINAPI HookGetSystemMetrics(int index)
{
    switch (index) {
    case SM_CXDOUBLECLK:
        return platform::input_settings::kDoubleClickWidth;
    case SM_CYDOUBLECLK:
        return platform::input_settings::kDoubleClickHeight;
    case SM_SWAPBUTTON:
        return platform::input_settings::kSwapButton;
    default:
        return g_realGetSystemMetrics(index);
    }
}

using SystemParametersInfoAFn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT);
SystemParametersInfoAFn g_realSystemParametersInfoA = nullptr;

BOOL WINAPI HookSystemParametersInfoA(UINT action, UINT param, PVOID value, UINT flags)
{
    switch (action) {
    case SPI_GETKEYBOARDDELAY:
    case SPI_GETKEYBOARDSPEED:
        if (!value) {
            return FALSE;
        }
        *static_cast<int*>(value) = action == SPI_GETKEYBOARDDELAY ? platform::input_settings::kKeyboardDelay
                                                                   : platform::input_settings::kKeyboardSpeed;
        return TRUE;
    case SPI_GETWHEELSCROLLLINES:
        if (!value) {
            return FALSE;
        }
        *static_cast<UINT*>(value) = platform::input_settings::kWheelScrollLines;
        return TRUE;
    default:
        return g_realSystemParametersInfoA(action, param, value, flags);
    }
}

// The game window's class (ChitinClass, CChitin::InitApplication in
// src/CChitin.cpp) is registered with GuardProc in front of its procedure,
// so the guard sees every message from the window's creation on, the
// activation sent while CreateWindowExA shows it included.
using RegisterClassExAFn = ATOM(WINAPI*)(const WNDCLASSEXA*);
RegisterClassExAFn g_realRegisterClassExA = nullptr;

ATOM WINAPI HookRegisterClassExA(const WNDCLASSEXA* wc)
{
    if (wc && wc->lpszClassName && HIWORD(reinterpret_cast<uintptr_t>(wc->lpszClassName))
        && _stricmp(wc->lpszClassName, "ChitinClass") == 0 && !g_gameProc.load()) {
        WNDCLASSEXA guarded = *wc;
        g_gameProc = wc->lpfnWndProc;
        guarded.lpfnWndProc = GuardProc;
        Log("window class %s registered behind the focus guard (%s)", wc->lpszClassName,
            g_focusGuard ? "on" : "off: test");
        return g_realRegisterClassExA(&guarded);
    }
    return g_realRegisterClassExA(wc);
}

using CreateWindowExAFn = HWND(WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE,
    LPVOID);
CreateWindowExAFn g_realCreateWindowExA = nullptr;

// Notes the game window as soon as it exists.
HWND WINAPI HookCreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR title, DWORD style, int x, int y, int width,
    int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
    HWND window = g_realCreateWindowExA(exStyle, className, title, style, x, y, width, height, parent, menu, instance,
        param);
    if (window && !parent && className && HIWORD(reinterpret_cast<uintptr_t>(className))
        && _stricmp(className, "ChitinClass") == 0) {
        g_window = window;
        Log("game window %p created", static_cast<void*>(window));
    }
    return window;
}

struct Hook {
    const char* dll;
    const char* name;
    void* replacement;
    void** real; // receives the original, or nullptr
    bool required; // the recorder cannot work without it
    bool hooked;
    // Installed only while this is nonzero (a test switch); nullptr: always.
    const unsigned* onlyIf = nullptr;
};

Hook g_hooks[] = {
    { "USER32.dll", "GetAsyncKeyState", reinterpret_cast<void*>(&HookGetAsyncKeyState), nullptr, true, false },
    { "USER32.dll", "GetCursorPos", reinterpret_cast<void*>(&HookGetCursorPos), nullptr, true, false },
    { "USER32.dll", "SetCursorPos", reinterpret_cast<void*>(&HookSetCursorPos), nullptr, false, false },
    { "USER32.dll", "PeekMessageA", reinterpret_cast<void*>(&HookPeekMessageA),
        reinterpret_cast<void**>(&g_realPeekMessageA), true, false },
    { "USER32.dll", "GetMessageA", reinterpret_cast<void*>(&HookGetMessageA),
        reinterpret_cast<void**>(&g_realGetMessageA), true, false },
    { "WINMM.dll", "timeGetTime", reinterpret_cast<void*>(&HookTime), nullptr, false, false },
    { "KERNEL32.dll", "GetTickCount", reinterpret_cast<void*>(&HookTime), nullptr, true, false },
    { "WINMM.dll", "timeSetEvent", reinterpret_cast<void*>(&HookTimeSetEvent), nullptr, true, false },
    { "KERNEL32.dll", "GetLocalTime", reinterpret_cast<void*>(&HookGetLocalTime), nullptr, true, false },
    { "KERNEL32.dll", "GetSystemTime", reinterpret_cast<void*>(&HookGetSystemTime), nullptr, true, false },
    { "KERNEL32.dll", "GetTimeZoneInformation", reinterpret_cast<void*>(&HookGetTimeZoneInformation), nullptr, true,
        false },
    { "WINMM.dll", "timeKillEvent", reinterpret_cast<void*>(&HookTimeKillEvent),
        reinterpret_cast<void**>(&g_realTimeKillEvent), false, false },
    { "KERNEL32.dll", "LoadLibraryA", reinterpret_cast<void*>(&HookLoadLibraryA),
        reinterpret_cast<void**>(&g_realLoadLibraryA), true, false },
    { "USER32.dll", "MessageBoxA", reinterpret_cast<void*>(&HookMessageBoxA), nullptr, false, false },
    // Only with IWD2R_TEST_SLOW_READS: otherwise the original's reads stay
    // untouched.
    { "KERNEL32.dll", "ReadFile", reinterpret_cast<void*>(&HookReadFile), reinterpret_cast<void**>(&g_realReadFile),
        false, false, &g_slowReadsMs },
    { "USER32.dll", "GetDoubleClickTime", reinterpret_cast<void*>(&HookGetDoubleClickTime), nullptr, true, false },
    { "USER32.dll", "GetSystemMetrics", reinterpret_cast<void*>(&HookGetSystemMetrics),
        reinterpret_cast<void**>(&g_realGetSystemMetrics), true, false },
    { "USER32.dll", "SystemParametersInfoA", reinterpret_cast<void*>(&HookSystemParametersInfoA),
        reinterpret_cast<void**>(&g_realSystemParametersInfoA), true, false },
    { "USER32.dll", "CreateWindowExA", reinterpret_cast<void*>(&HookCreateWindowExA),
        reinterpret_cast<void**>(&g_realCreateWindowExA), true, false },
    { "USER32.dll", "RegisterClassExA", reinterpret_cast<void*>(&HookRegisterClassExA),
        reinterpret_cast<void**>(&g_realRegisterClassExA), true, false },
};

bool WriteCode(uint32_t address, const void* bytes, size_t size)
{
    void* target = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    DWORD old = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old)) {
        return false;
    }
    std::memcpy(target, bytes, size);
    VirtualProtect(target, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    return true;
}

// Points the exe's IAT slots for g_hooks at the hooks.
bool HookImports(std::string* error)
{
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); desc->Name;
         ++desc) {
        const char* dll = reinterpret_cast<const char*>(base + desc->Name);
        if (!desc->OriginalFirstThunk) {
            continue;
        }
        const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + desc->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG32) {
                continue;
            }
            const auto* byName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            for (Hook& hook : g_hooks) {
                if ((hook.onlyIf && !*hook.onlyIf) || _stricmp(dll, hook.dll) != 0 || std::strcmp(reinterpret_cast<const char*>(byName->Name), hook.name)
                    || hook.hooked) {
                    continue;
                }
                const uint32_t slot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&slots->u1.Function));
                const uint32_t real = slots->u1.Function;
                if (hook.real) {
                    *hook.real = reinterpret_cast<void*>(static_cast<uintptr_t>(real));
                }
                const uint32_t replacement = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook.replacement));
                if (!WriteCode(slot, &replacement, 4)) {
                    *error = std::string("cannot write the IAT slot of ") + hook.name;
                    return false;
                }
                hook.hooked = true;
                Log("hooked %s!%s: IAT slot 0x%08X (was 0x%08X)", hook.dll, hook.name, slot, real);
            }
        }
    }
    for (const Hook& hook : g_hooks) {
        if (hook.hooked || (hook.onlyIf && !*hook.onlyIf)) {
            continue;
        }
        if (hook.required) {
            *error = std::string("the exe does not import ") + hook.dll + "!" + hook.name;
            return false;
        }
        Log("%s!%s: not imported by the exe, nothing to hook", hook.dll, hook.name);
    }
    return true;
}

// Replaces the jmp at the frame hook site with a jmp to FrameTrampoline,
// after checking the site holds exactly the recorded bytes.
bool HookFrame(std::string* error)
{
    uint8_t expected[5];
    for (int i = 0; i < 5; ++i) {
        unsigned value = 0;
        std::sscanf(kVanillaFrameHookBytes + 2 * i, "%2x", &value);
        expected[i] = static_cast<uint8_t>(value);
    }
    const auto* site = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(kVanillaFrameHook));
    if (std::memcmp(site, expected, 5) != 0) {
        char text[160];
        std::snprintf(text, sizeof(text), "the frame hook site 0x%08X holds %02X %02X %02X %02X %02X, not %s",
            kVanillaFrameHook, site[0], site[1], site[2], site[3], site[4], kVanillaFrameHookBytes);
        *error = text;
        return false;
    }
    int32_t rel = 0;
    std::memcpy(&rel, site + 1, 4);
    g_frameTarget = kVanillaFrameHook + 5 + static_cast<uint32_t>(rel);
    if (g_frameTarget != kVanillaFrameHookTarget) {
        *error = "the frame hook's jmp does not go to CChitin::SynchronousUpdate";
        return false;
    }
    uint8_t jump[5] = { 0xE9 };
    const uint32_t newRel = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&FrameTrampoline)) - (kVanillaFrameHook + 5);
    std::memcpy(jump + 1, &newRel, 4);
    if (!WriteCode(kVanillaFrameHook, jump, sizeof(jump))) {
        *error = "cannot write the frame hook";
        return false;
    }
    Log("frame hook: 0x%08X jmp 0x%08X -> jmp FrameTrampoline 0x%08X", kVanillaFrameHook, g_frameTarget,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&FrameTrampoline)));
    return true;
}

// IWD2R_RAND_TRACE: replaces rand's call to _getptd with a jmp to
// RandTrampoline, after checking the site holds that call.
bool HookRand(std::string* error)
{
    const auto* site = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(kVanillaRand));
    int32_t rel = 0;
    std::memcpy(&rel, site + 1, 4);
    if (site[0] != 0xE8 || kVanillaRand + 5 + static_cast<uint32_t>(rel) != kVanillaGetptd) {
        *error = "rand does not begin with a call to _getptd";
        return false;
    }
    InitializeCriticalSection(&g_randLock);
    uint8_t jump[5] = { 0xE9 };
    const uint32_t newRel = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&RandTrampoline)) - (kVanillaRand + 5);
    std::memcpy(jump + 1, &newRel, 4);
    if (!WriteCode(kVanillaRand, jump, sizeof(jump))) {
        *error = "cannot write the rand hook";
        return false;
    }
    g_randTrace = true;
    Log("rand trace: rand 0x%08X -> RandTrampoline, calls in rand.txt", kVanillaRand);
    return true;
}

bool Start()
{
    g_dir = Env("IWD2R_REPLAY_DIR");
    while (!g_dir.empty() && (g_dir.back() == '\\' || g_dir.back() == '/')) {
        g_dir.pop_back();
    }
    if (g_dir.empty()) {
        return false; // nowhere to say why; the injector reports the failed load
    }
    g_log = std::fopen(PathIn("recorder.log").c_str(), "w");
    Log("recorder.dll in process %lu, replay folder %s", GetCurrentProcessId(), g_dir.c_str());
    const std::string every = Env("IWD2R_DUMP_EVERY");
    if (!every.empty() && !ParseUnsigned(every, &g_dumpEvery)) {
        Log("FATAL: IWD2R_DUMP_EVERY=%s is not a number", every.c_str());
        return false;
    }
    const std::string seed = Env("IWD2R_SEED");
    if (!seed.empty() && !ParseUnsigned(seed, &g_seed)) {
        Log("FATAL: IWD2R_SEED=%s is not a number", seed.c_str());
        return false;
    }
    const std::string steal = Env("IWD2R_TEST_STEAL_FOCUS");
    if (!steal.empty()) {
        unsigned value = 0;
        if (!ParseUnsigned(steal, &value) || value == 0) {
            Log("FATAL: IWD2R_TEST_STEAL_FOCUS=%s is not a frame number", steal.c_str());
            return false;
        }
        g_stealFocusFrame = value;
        Log("test: the focus will be stolen at frame %u", value);
    }
    const std::string slow = Env("IWD2R_TEST_SLOW_READS");
    if (!slow.empty()) {
        if (!ParseUnsigned(slow, &g_slowReadsMs) || g_slowReadsMs == 0) {
            Log("FATAL: IWD2R_TEST_SLOW_READS=%s is not a number of ms", slow.c_str());
            return false;
        }
        Log("test: ReadFile off the main thread sleeps %u ms first", g_slowReadsMs);
    }
    if (!Env("IWD2R_TEST_NO_FOCUS_GUARD").empty()) {
        g_focusGuard = false;
        Log("test: focus guard OFF (activation messages reach the game)");
    }
    const std::string script = Env("IWD2R_SCRIPT");
    if (!script.empty()) {
        std::FILE* in = std::fopen(script.c_str(), "rb");
        if (!in) {
            Log("FATAL: %s: cannot read the scenario", script.c_str());
            return false;
        }
        std::string text;
        char buffer[4096];
        for (size_t n; (n = std::fread(buffer, 1, sizeof(buffer), in)) > 0;) {
            text.append(buffer, n);
        }
        std::fclose(in);
        std::string error;
        size_t count = 0;
        if (!recorder::LoadScript(text, &error, &count)) {
            Log("FATAL: %s: %s", script.c_str(), error.c_str());
            return false;
        }
        g_haveScript = true;
        Log("scenario %s (%zu commands)", script.c_str(), count);
    }
    Log("dumps every %u frames, seed %u, virtual clock from %lu ms at %lu frames per second", g_dumpEvery, g_seed,
        kClockBase, kFramesPerSecond);
    std::string error;
    const std::string randTrace = Env("IWD2R_RAND_TRACE");
    if (!randTrace.empty() && randTrace != "0" && !HookRand(&error)) {
        Log("FATAL: %s", error.c_str());
        return false;
    }
    if (!HookImports(&error) || !HookFrame(&error)) {
        Log("FATAL: %s", error.c_str());
        return false;
    }
    WriteStatus();
    return true;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (!Start()) {
            if (g_log) {
                std::fclose(g_log);
                g_log = nullptr;
            }
            return FALSE;
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        Log("process exiting");
    }
    return TRUE;
}
