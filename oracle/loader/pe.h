// iwd2r. Distributed under LICENSE.md.
// PE32 parsing (portable) and, on 32-bit Windows, mapping the vanilla iwd2.exe
// at its own image base so its code runs in the oracle process.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PeSection {
    std::string name;
    uint32_t rva = 0;
    uint32_t virtualSize = 0;
    uint32_t rawOffset = 0;
    uint32_t rawSize = 0;
    uint32_t characteristics = 0;
};

struct PeImport {
    std::string dll;
    std::string name; // empty when imported by ordinal
    uint16_t ordinal = 0; // the ordinal, or the hint when imported by name
    uint32_t iatRva = 0;
};

struct PeImage {
    uint32_t imageBase = 0;
    uint32_t imageSize = 0;
    uint32_t entryRva = 0;
    uint32_t headersSize = 0; // SizeOfHeaders: the bytes mapped at imageBase
    std::vector<PeSection> sections;
    std::vector<PeImport> imports;
};

// Parses a PE32 (not PE32+) image file: headers, section table and import
// directory. Every offset is bounds-checked; on failure returns false and sets
// *error to a one-line reason.
bool ParsePe(const std::vector<uint8_t>& file, PeImage* out, std::string* error);

#if defined(_WIN32) && defined(_M_IX86)
// Reserves [imageBase, imageBase + imageSize) of the vanilla exe in process,
// a runner child created suspended: in a running process Windows has already
// mapped NLS tables at 0x400000 before any TLS callback (C_1252.NLS was found
// there). On failure *occupant names the module, mapped file or memory kind
// holding the first conflicting region.
bool ReserveImageRange(void* process, std::string* occupant);

// In the child: true when the whole range is the parent's untouched
// reservation, else false with *error naming what is there instead.
bool CheckImageRangeReserved(std::string* error);

// Commits the reserved range, copies the headers and sections, zero-fills
// virtual sizes past raw sizes and applies section protections.
bool MapImage(const PeImage& image, const std::vector<uint8_t>& file, std::string* error);

// Fills the IAT with LoadLibraryA/GetProcAddress results. An import that does
// not resolve is bound to a trap stub that names it and exits with code 3 when
// called; the unresolved list is printed to stderr once.
bool ResolveImports(const PeImage& image, std::string* error);

// Rewrites the CRT's `call WinMain` at callSite to call replacement, after
// checking the opcode is 0xE8 and the rel32 target is expectedTarget.
bool PatchWinMainCall(uint32_t callSite, uint32_t expectedTarget, void* replacement, std::string* error);

// Runs the image's entry point (its CRT startup) on a dedicated thread. The
// CRT's exit() ends the process; returning from the thread is an error.
[[noreturn]] void EnterImage(const PeImage& image);
#endif
