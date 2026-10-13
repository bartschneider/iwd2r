// iwd2r. Distributed under LICENSE.md.
// Maps the vanilla iwd2.exe at 0x400000 in the 32-bit oracle process.
#include "pe.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <psapi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

// The vanilla exe's range. It has no relocations, so it runs only here.
constexpr uint32_t kImageBase = 0x400000;
constexpr uint32_t kImageSize = 0x621000;
constexpr uint32_t kPage = 0x1000;
constexpr uint32_t kStackReserve = 0x100000; // the exe's SizeOfStackReserve

constexpr uint32_t kExecute = 0x20000000;
constexpr uint32_t kRead = 0x40000000;
constexpr uint32_t kWrite = 0x80000000;

std::string Hex(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", value);
    return text;
}

bool Fail(std::string* error, const std::string& reason)
{
    if (error) {
        *error = reason;
    }
    return false;
}

std::string LastError() { return "error " + std::to_string(GetLastError()); }

uint32_t AlignUp(uint32_t value) { return (value + kPage - 1) & ~(kPage - 1); }

DWORD Protection(uint32_t characteristics)
{
    const bool x = characteristics & kExecute;
    const bool w = characteristics & kWrite;
    const bool r = characteristics & kRead;
    if (x) {
        return w ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    }
    if (w) {
        return PAGE_READWRITE;
    }
    return r ? PAGE_READONLY : PAGE_NOACCESS;
}

// Copies bytes into the mapped image, lifting page protection for the write.
bool WriteImage(uint32_t address, const void* data, size_t size, std::string* error)
{
    void* target = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    DWORD old = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old)) {
        return Fail(error, "VirtualProtect " + Hex(address) + " failed: " + LastError());
    }
    std::memcpy(target, data, size);
    DWORD ignored = 0;
    VirtualProtect(target, size, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    return true;
}

// Imports that did not resolve, indexed by trap stub.
std::vector<std::string>& Unresolved()
{
    static std::vector<std::string> names;
    return names;
}

extern "C" void __cdecl UnresolvedImportTrap(uint32_t index)
{
    std::fflush(stdout);
    std::fprintf(stderr, "oracle: called unresolved import %s\n", Unresolved()[index].c_str());
    std::fflush(stderr);
    ExitProcess(3);
}

// One stub per unresolved import: push index; mov eax, trap; call eax.
constexpr size_t kStubSize = 12;

bool MakeTrapStubs(std::vector<uint32_t>* stubs, std::string* error)
{
    const size_t count = Unresolved().size();
    if (count == 0) {
        return true;
    }
    auto* code = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, count * kStubSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!code) {
        return Fail(error, "cannot allocate import trap stubs: " + LastError());
    }
    const uint32_t trap = reinterpret_cast<uint32_t>(&UnresolvedImportTrap);
    for (size_t i = 0; i < count; ++i) {
        uint8_t* stub = code + i * kStubSize;
        const uint32_t index = static_cast<uint32_t>(i);
        stub[0] = 0x68;
        std::memcpy(stub + 1, &index, 4);
        stub[5] = 0xB8;
        std::memcpy(stub + 6, &trap, 4);
        stub[10] = 0xFF;
        stub[11] = 0xD0;
        stubs->push_back(reinterpret_cast<uint32_t>(stub));
    }
    DWORD old = 0;
    if (!VirtualProtect(code, count * kStubSize, PAGE_EXECUTE_READ, &old)) {
        return Fail(error, "cannot protect import trap stubs: " + LastError());
    }
    FlushInstructionCache(GetCurrentProcess(), code, count * kStubSize);
    return true;
}

std::string ImportName(const PeImport& import)
{
    return import.dll + "!" + (import.name.empty() ? "#" + std::to_string(import.ordinal) : import.name);
}

DWORD WINAPI EntryThread(LPVOID entry)
{
    reinterpret_cast<void(__cdecl*)()>(entry)();
    return 0;
}

// Names the first region of [kImageBase, kImageBase + kImageSize) in process
// that is not free, or returns false when the whole range is free.
bool DescribeOccupant(HANDLE process, std::string* occupant)
{
    for (uint32_t address = kImageBase; address < kImageBase + kImageSize;) {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQueryEx(process, reinterpret_cast<void*>(static_cast<uintptr_t>(address)), &info, sizeof(info))
            == 0) {
            return false;
        }
        const uint32_t regionBase = reinterpret_cast<uint32_t>(info.BaseAddress);
        const uint32_t regionEnd = regionBase + static_cast<uint32_t>(info.RegionSize);
        if (info.State != MEM_FREE) {
            char name[MAX_PATH] = {};
            std::string what;
            if (info.Type == MEM_IMAGE
                && GetModuleFileNameExA(process, static_cast<HMODULE>(info.AllocationBase), name, sizeof(name))) {
                what = std::string("module ") + name;
            } else if (info.Type != MEM_PRIVATE && GetMappedFileNameA(process, info.BaseAddress, name, sizeof(name))) {
                what = std::string(info.Type == MEM_IMAGE ? "image " : "mapped view of ") + name;
            } else {
                what = info.Type == MEM_IMAGE ? "unnamed image"
                    : info.Type == MEM_MAPPED ? "unnamed mapped view"
                                              : "private memory";
            }
            *occupant = what + " at " + Hex(regionBase) + "-" + Hex(regionEnd) + " (allocation base "
                + Hex(reinterpret_cast<uint32_t>(info.AllocationBase)) + ")";
            return true;
        }
        address = regionEnd;
    }
    return false;
}

} // namespace

bool ReserveImageRange(void* process, std::string* occupant)
{
    void* base = reinterpret_cast<void*>(static_cast<uintptr_t>(kImageBase));
    if (VirtualAllocEx(process, base, kImageSize, MEM_RESERVE, PAGE_NOACCESS) == base) {
        return true;
    }
    const DWORD reserveError = GetLastError();
    if (!DescribeOccupant(process, occupant)) {
        *occupant = "unknown (VirtualAllocEx error " + std::to_string(reserveError) + ")";
    }
    return false;
}

bool CheckImageRangeReserved(std::string* error)
{
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(reinterpret_cast<void*>(static_cast<uintptr_t>(kImageBase)), &info, sizeof(info)) != 0
        && info.State == MEM_RESERVE && info.Type == MEM_PRIVATE
        && reinterpret_cast<uint32_t>(info.AllocationBase) == kImageBase && info.RegionSize >= kImageSize) {
        return true;
    }
    std::string occupant;
    if (!DescribeOccupant(GetCurrentProcess(), &occupant)) {
        occupant = "nothing (the range is free)";
    }
    return Fail(error, "the image range is not reserved: found " + occupant);
}

bool MapImage(const PeImage& image, const std::vector<uint8_t>& file, std::string* error)
{
    if (image.imageBase != kImageBase || image.imageSize != kImageSize) {
        return Fail(error,
            "image is " + Hex(image.imageBase) + "+" + Hex(image.imageSize) + ", the reserved range is "
                + Hex(kImageBase) + "+" + Hex(kImageSize));
    }
    auto* base = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(kImageBase)),
        kImageSize, MEM_COMMIT, PAGE_READWRITE));
    if (!base) {
        return Fail(error, "cannot commit the image range: " + LastError());
    }
    const size_t headers = std::min<size_t>(image.headersSize, file.size());
    std::memcpy(base, file.data(), headers);
    for (const PeSection& s : image.sections) {
        const uint32_t mapped = s.virtualSize != 0 ? s.virtualSize : s.rawSize;
        std::memcpy(base + s.rva, file.data() + s.rawOffset, std::min(s.rawSize, mapped));
    }
    DWORD old = 0;
    if (!VirtualProtect(base, AlignUp(image.headersSize), PAGE_READONLY, &old)) {
        return Fail(error, "cannot protect the headers: " + LastError());
    }
    for (const PeSection& s : image.sections) {
        const uint32_t mapped = s.virtualSize != 0 ? s.virtualSize : s.rawSize;
        if (!VirtualProtect(base + s.rva, AlignUp(mapped), Protection(s.characteristics), &old)) {
            return Fail(error, "cannot protect section " + s.name + ": " + LastError());
        }
    }
    return true;
}

bool ResolveImports(const PeImage& image, std::string* error)
{
    Unresolved().clear();
    std::vector<uint32_t> addresses(image.imports.size());
    std::vector<size_t> unresolvedIndex(image.imports.size(), SIZE_MAX);
    std::string loadedDll;
    HMODULE module = nullptr;
    for (size_t i = 0; i < image.imports.size(); ++i) {
        const PeImport& import = image.imports[i];
        if (import.iatRva < image.headersSize || uint64_t(import.iatRva) + 4 > image.imageSize) {
            return Fail(error, "IAT slot of " + ImportName(import) + " is outside the image");
        }
        if (import.dll != loadedDll) {
            loadedDll = import.dll;
            module = LoadLibraryA(import.dll.c_str());
        }
        FARPROC proc = nullptr;
        if (module) {
            proc = GetProcAddress(module,
                import.name.empty() ? MAKEINTRESOURCEA(import.ordinal) : import.name.c_str());
        }
        if (proc) {
            addresses[i] = reinterpret_cast<uint32_t>(proc);
        } else {
            unresolvedIndex[i] = Unresolved().size();
            Unresolved().push_back(ImportName(import));
        }
    }
    std::vector<uint32_t> stubs;
    if (!MakeTrapStubs(&stubs, error)) {
        return false;
    }
    if (!Unresolved().empty()) {
        std::fprintf(stderr, "oracle: %zu unresolved imports bound to traps:", Unresolved().size());
        for (const std::string& name : Unresolved()) {
            std::fprintf(stderr, " %s", name.c_str());
        }
        std::fprintf(stderr, "\n");
    }
    for (size_t i = 0; i < image.imports.size(); ++i) {
        const uint32_t value = unresolvedIndex[i] == SIZE_MAX ? addresses[i] : stubs[unresolvedIndex[i]];
        if (!WriteImage(image.imageBase + image.imports[i].iatRva, &value, 4, error)) {
            return false;
        }
    }
    return true;
}

bool PatchWinMainCall(uint32_t callSite, uint32_t expectedTarget, void* replacement, std::string* error)
{
    if (callSite < kImageBase || callSite + 5 > kImageBase + kImageSize) {
        return Fail(error, "call site " + Hex(callSite) + " is outside the image");
    }
    const auto* code = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(callSite));
    if (code[0] != 0xE8) {
        char opcode[8];
        std::snprintf(opcode, sizeof(opcode), "0x%02X", code[0]);
        return Fail(error, "byte at " + Hex(callSite) + " is " + opcode + ", not a call (0xE8)");
    }
    int32_t rel = 0;
    std::memcpy(&rel, code + 1, 4);
    const uint32_t target = callSite + 5 + static_cast<uint32_t>(rel);
    if (target != expectedTarget) {
        return Fail(error,
            "call at " + Hex(callSite) + " targets " + Hex(target) + ", expected WinMain at " + Hex(expectedTarget));
    }
    const uint32_t newRel = reinterpret_cast<uint32_t>(replacement) - (callSite + 5);
    return WriteImage(callSite + 1, &newRel, 4, error);
}

void EnterImage(const PeImage& image)
{
    void* entry = reinterpret_cast<void*>(static_cast<uintptr_t>(image.imageBase + image.entryRva));
    std::fflush(stdout);
    std::fflush(stderr);
    HANDLE thread = CreateThread(nullptr, kStackReserve, EntryThread, entry, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!thread) {
        std::fprintf(stderr, "oracle: cannot start the image's entry thread: %s\n", LastError().c_str());
        ExitProcess(2);
    }
    WaitForSingleObject(thread, INFINITE);
    std::fprintf(stderr, "oracle: the image's CRT entry returned instead of exiting\n");
    std::fflush(stderr);
    ExitProcess(2);
}
