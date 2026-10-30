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

constexpr DWORD kCxxExceptionCode = 0xE06D7363; // 'msc' | 0xE0000000
constexpr uint32_t kVanillaImageStart = 0x400000;
constexpr uint32_t kVanillaImageEnd = 0xA21000;

void* Address(uint32_t address)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

bool InImage(uint32_t address, uint32_t size)
{
    return address >= kVanillaImageStart && address < kVanillaImageEnd && kVanillaImageEnd - address >= size;
}

uint32_t ImageDword(uint32_t address)
{
    return *reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(address));
}

// The mangled type name of the first catchable type of an original ThrowInfo
// (ThrowInfo.pCatchableTypeArray -> arrayOfCatchableTypes[0] -> pType ->
// TypeDescriptor.name), or "?" when a pointer leaves the image.
void ThrowTypeName(uint32_t throwInfo, char* out, size_t size)
{
    std::snprintf(out, size, "?");
    if (!InImage(throwInfo, 16)) {
        return;
    }
    const uint32_t array = ImageDword(throwInfo + 12);
    if (!InImage(array, 8) || ImageDword(array) == 0) {
        return;
    }
    const uint32_t catchable = ImageDword(array + 4);
    if (!InImage(catchable, 8)) {
        return;
    }
    const uint32_t descriptor = ImageDword(catchable + 4);
    if (!InImage(descriptor, 9)) {
        return;
    }
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(descriptor + 8));
    size_t length = 0;
    while (length + 1 < size && InImage(descriptor + 8 + static_cast<uint32_t>(length), 1) && name[length] != 0) {
        out[length] = name[length];
        ++length;
    }
    out[length] = 0;
}

DWORD Filter(const EXCEPTION_POINTERS* info, GuardedFault* fault)
{
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    fault->code = record->ExceptionCode;
    fault->address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record->ExceptionAddress));
    fault->originalThrow = false;
    fault->type[0] = 0;
    if (record->ExceptionCode == kCxxExceptionCode) {
        // ExceptionInformation: magic, the thrown object, its ThrowInfo.
        const uint32_t throwInfo
            = record->NumberParameters >= 3 ? static_cast<uint32_t>(record->ExceptionInformation[2]) : 0;
        if (!InImage(throwInfo, 16)) {
            return EXCEPTION_CONTINUE_SEARCH; // the runner's own throw
        }
        fault->originalThrow = true;
        ThrowTypeName(throwInfo, fault->type, sizeof(fault->type));
    }
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

bool CallGuarded(void (*thunk)(void*), void* arg, GuardedFault* fault)
{
    __try {
        thunk(arg);
        return true;
    } __except (Filter(GetExceptionInformation(), fault)) {
        return false;
    }
}

void InitializeOriginalCriticalSection(void* section)
{
    InitializeCriticalSection(static_cast<LPCRITICAL_SECTION>(section));
}

bool OriginalHeapValid()
{
    __try {
        const HANDLE heap = *reinterpret_cast<const HANDLE*>(static_cast<uintptr_t>(kVanillaCrtHeap));
        return heap != nullptr && HeapValidate(heap, 0, nullptr) != FALSE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
