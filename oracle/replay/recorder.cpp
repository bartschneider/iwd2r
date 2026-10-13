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
//
// The frame: the original's main thread calls CBaldurChitin::SynchronousUpdate
// once per AI update (vanilla.json "frame_hook"). Its first instruction, a
// 5-byte jmp to CChitin::SynchronousUpdate, becomes a jmp to FrameTrampoline,
// which calls OnFrame and then takes the original jmp. OnFrame counts the
// frame, runs the scenario step, writes the due dumps and status.txt, then
// advances the virtual clock and fires the due timeSetEvent callbacks, which
// wake the AI thread for the next AsynchronousUpdate. Dumps are written only
// there, on the main thread, while the AI thread waits for that wake-up.
//
// Hooked imports (the exe's IAT): input GetAsyncKeyState, GetCursorPos,
// SetCursorPos, PeekMessageA, GetMessageA; time timeGetTime, GetTickCount,
// timeSetEvent, timeKillEvent, and the wall clock GetLocalTime,
// GetSystemTime, GetTimeZoneInformation (VC6's time() reads them, and the
// original seeds its AI thread's rand with time()); and LoadLibraryA (binkw32 refused, so the
// movies are skipped as in our engine, whose LoadLibraryA returns NULL) and
// MessageBoxA (answered with its first button without showing it, as our
// engine does headless). An import the exe does not have is logged and
// skipped. Real keyboard and mouse input never reaches the game: the hooks
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

// The virtual clock: kClockBase at the start, then 1000 / kFramesPerSecond
// ms per frame (CChitin::TIMER_UPDATES_PER_SECOND, 30 by default), exact on
// average (frame N reads kClockBase + N * 1000 / 30).
constexpr DWORD kClockBase = 0x01000000;
constexpr DWORD kFramesPerSecond = 30;

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
std::atomic<DWORD> g_now { kClockBase };
DWORD g_mainThread = 0;
HWND g_window = nullptr;
std::string g_dir;
unsigned g_dumpEvery = 30;
unsigned g_seed = 1;
bool g_haveScript = false;
bool g_scriptFinished = false;
std::atomic<uint32_t> g_inputTiddata { 0 }; // _tiddata of the thread that polls input
std::atomic<int> g_lastX { 400 }; // the cursor in client pixels when the scenario does not say
std::atomic<int> g_lastY { 300 };
uint32_t g_frameTarget = 0; // where the patched jmp went: CChitin::SynchronousUpdate

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

PeekMessageAFn g_realPeekMessageA = nullptr;
GetMessageAFn g_realGetMessageA = nullptr;
TimeKillEventFn g_realTimeKillEvent = nullptr;
LoadLibraryAFn g_realLoadLibraryA = nullptr;

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

// The object the create_char screen edits: CGameObjectArray::GetShare's
// THREAD_ASYNCH lookup, without the call. 0 when the id does not resolve.
uint32_t ChargenSprite(uint32_t chitin, int32_t id)
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
    const uint32_t sprite = ReadU32(At(entry, kField_object_entry_ptr), &ok);
    return ok ? sprite : 0;
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
    const uint32_t sprite = ChargenSprite(chitin, spriteId);
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
        + ", \"in_async_update\": " + Number(inAsync) + ", \"ai_rng_state\": "
        + (inputTiddata && aiOk ? Number(aiRng) : std::string("null")) + "},\n";
    const bool chargen = std::strcmp(screen, "create_char") == 0;
    json += "  \"chargen\": " + (chargen ? ChargenJson(chitin, active) : std::string("null")) + "\n}\n";
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
    if (!g_window || !GetClientRect(g_window, &rect) || rect.right <= 0 || rect.bottom <= 0) {
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
    HDC windowDc = GetDC(g_window);
    const BOOL printed = BitBlt(memory, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);
    ReleaseDC(g_window, windowDc);
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
        msg.hwnd = g_window;
        msg.message = WM_CLOSE;
        msg.time = g_now.load();
        msg.pt.x = g_lastX.load();
        msg.pt.y = g_lastY.load();
        std::lock_guard<std::mutex> lock(g_messageMutex);
        g_messages.push_back(msg);
        Log("line %d: quit: WM_CLOSE queued for window %p", line, static_cast<void*>(g_window));
    }

    void Checkpoint(const std::string& name, int line) override
    {
        Log("line %d: checkpoint %s", line, name.c_str());
        WriteDump(name.c_str());
    }
};

BOOL CALLBACK FindGameWindow(HWND window, LPARAM)
{
    RECT rect;
    if (IsWindowVisible(window) && GetClientRect(window, &rect) && rect.right > 0 && rect.bottom > 0) {
        g_window = window;
        return FALSE;
    }
    return TRUE;
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

extern "C" void __cdecl OnFrame()
{
    const uint32_t frame = ++g_frame;
    if (frame == 1) {
        g_mainThread = GetCurrentThreadId();
        reinterpret_cast<void(__cdecl*)(unsigned int)>(static_cast<uintptr_t>(kVanillaSrand))(g_seed);
        Log("first frame on thread %lu; srand(%u) on it", g_mainThread, g_seed);
    }
    if (!g_window) {
        EnumThreadWindows(GetCurrentThreadId(), FindGameWindow, 0);
        if (g_window) {
            RECT rect;
            GetClientRect(g_window, &rect);
            Log("game window %p, client %ldx%ld", static_cast<void*>(g_window), rect.right, rect.bottom);
        }
    }
    if (frame == 1 && g_haveScript) {
        int width = 800;
        int height = 600;
        RECT rect;
        if (g_window && GetClientRect(g_window, &rect)) {
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
    if (g_dumpEvery && frame % g_dumpEvery == 0) {
        WriteDump(nullptr);
    }
    WriteStatus();
    g_now = kClockBase + static_cast<DWORD>(static_cast<uint64_t>(frame) * 1000 / kFramesPerSecond);
    FireTimers();
}

// Replaces the first instruction of CBaldurChitin::SynchronousUpdate. It runs
// OnFrame with every register and flag kept, then takes the original jmp, so
// SynchronousUpdate sees its `this` (ecx) and return address untouched.
extern "C" __declspec(naked) void FrameTrampoline()
{
    __asm {
        pushad
        pushfd
        call OnFrame
        popfd
        popad
        jmp dword ptr [g_frameTarget]
    }
}

// ---------------------------------------------------------------------------
// Hooks

void NoteInputThread()
{
    if (g_mainThread != 0 && GetCurrentThreadId() != g_mainThread && g_inputTiddata.load() == 0) {
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
    if (g_window) {
        ClientToScreen(g_window, point);
    }
    return TRUE;
}

BOOL WINAPI HookSetCursorPos(int x, int y)
{
    // The real cursor stays where the user has it; the game's own moves
    // become the scenario cursor until the scenario sets one.
    POINT point = { x, y };
    if (g_window) {
        ScreenToClient(g_window, &point);
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
    if (GetCurrentThreadId() != g_mainThread) {
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

// The wall clock: kWallClockStart plus the virtual clock's milliseconds, the
// same as local and as UTC time (GetTimeZoneInformation answers UTC). The
// original seeds the AI thread's rand with srand(time(NULL)) when the main
// menu starts (CScreenConnection::TimerAsynchronousUpdate, 0x5FB3E0), and
// VC6's time() reads GetLocalTime and the time zone.
void VirtualWallClock(LPSYSTEMTIME out)
{
    SYSTEMTIME start = {};
    start.wYear = 2002;
    start.wMonth = 8;
    start.wDay = 27; // Icewind Dale II's release
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

struct Hook {
    const char* dll;
    const char* name;
    void* replacement;
    void** real; // receives the original, or nullptr
    bool required; // the recorder cannot work without it
    bool hooked;
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
                if (_stricmp(dll, hook.dll) != 0 || std::strcmp(reinterpret_cast<const char*>(byName->Name), hook.name)
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
        if (hook.hooked) {
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
