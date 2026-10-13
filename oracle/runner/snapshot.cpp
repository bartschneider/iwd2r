// iwd2r. Distributed under LICENSE.md.
// The .data snapshot of the original, its rand state, and the runner's one
// __try/__except, which CaseContext::CallOriginal goes through.
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

struct Segment {
    uint32_t start;
    std::vector<uint8_t> bytes;
};

std::vector<Segment>& Segments()
{
    static std::vector<Segment> segments;
    return segments;
}

// The main thread's _tiddata and its saved rand fields.
uint8_t* g_ptd = nullptr;
uint8_t g_ptdRand[kVanillaTiddataRandEnd - kVanillaTiddataRandStart];
DWORD g_snapshotThread = 0;

void* Address(uint32_t address)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

DWORD Filter(const EXCEPTION_POINTERS* info, uint32_t* code, uint32_t* address)
{
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    if (record->ExceptionCode == 0xE06D7363) {
        return EXCEPTION_CONTINUE_SEARCH; // a C++ throw, not a fault
    }
    *code = record->ExceptionCode;
    *address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record->ExceptionAddress));
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

bool TakeSnapshot(DataRange data, std::string* error)
{
    Segments().clear();
    uint32_t cursor = data.start;
    auto keep = [&](uint32_t end) {
        if (end > cursor) {
            Segment segment { cursor, std::vector<uint8_t>(end - cursor) };
            std::memcpy(segment.bytes.data(), Address(cursor), segment.bytes.size());
            Segments().push_back(std::move(segment));
        }
    };
    for (const auto& range : kVanillaCrtRanges) {
        if (range[0] < cursor || range[1] > data.end) {
            *error = "CRT range is not sorted or not inside .data";
            return false;
        }
        keep(range[0]);
        cursor = range[1];
    }
    keep(data.end);
    using GetPtd = uint8_t*(__cdecl*)();
    g_ptd = reinterpret_cast<GetPtd>(static_cast<uintptr_t>(kVanillaGetptd))();
    std::memcpy(g_ptdRand, g_ptd + kVanillaTiddataRandStart, sizeof(g_ptdRand));
    g_snapshotThread = GetCurrentThreadId();
    return true;
}

void RestoreData()
{
    if (GetCurrentThreadId() != g_snapshotThread) {
        std::fprintf(stderr, "oracle: RestoreData called off the snapshot thread\n");
        ExitProcess(8);
    }
    for (const Segment& segment : Segments()) {
        std::memcpy(Address(segment.start), segment.bytes.data(), segment.bytes.size());
    }
    std::memcpy(g_ptd + kVanillaTiddataRandStart, g_ptdRand, sizeof(g_ptdRand));
}

bool CallGuarded(void (*thunk)(void*), void* arg, uint32_t* code, uint32_t* address)
{
    __try {
        thunk(arg);
        return true;
    } __except (Filter(GetExceptionInformation(), code, address)) {
        return false;
    }
}
