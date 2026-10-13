// The video layer's pixel functions, the original's against ours, on plain
// buffers.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures (R10a, R10b). The buffers the original writes are blocks from its
// operator new, leaked; ours writes into a copy holding the same pixels.
// CVidPalette::RealizeRange (0x7BF870) reads the current video mode through
// g_pChitin (0x8CF6D8) -> pActiveEngine (0x3C4) -> pVidMode (4), the AU
// counter at 0x190E and the 3d flag at 0x91C; the chitin and the engine are
// zeroed blocks holding only those members. The video mode is a CVidInf from
// the original's constructor (0x79AF00), because the palette's SetType calls
// its GetTransparentColor through the vtable, with the members the function
// reads set afterwards. The palettes come from the original's CVidPalette
// constructor (0x7BEEA0). Ours gets the same: zeroed CChitin and CWarp
// storage, our CVidInf and CVidPalette from their constructors; g_pChitin is
// put back when the case ends. Original objects are never freed.
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CChitin.h"
#include "CVidInf.h"
#include "CVidPalette.h"
#include "CVidPoly.h"
#include "CWarp.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CVidPoly::DrawHLineShaded32 (0x7D6EB0) and DrawHLineShadedMirrored32
// (0x7D6FD0): (pSurface, xMin, xMax, dwColor, rSurface, ptRef), `ret 0x18`
// at 0x7D6FC3 and 0x7D70ED. Neither reads ecx; the mirrored one reads
// rSurface's left and right.
using HLineFn = void(__fastcall*)(void* self, void* edx, void* pSurface, int32_t xMin, int32_t xMax,
    uint32_t dwColor, const void* rSurface, const void* ptRef);
constexpr uintptr_t kDrawHLineShaded32 = 0x7D6EB0;
constexpr uintptr_t kDrawHLineShadedMirrored32 = 0x7D6FD0;
// CVidPoly: field_0, m_pVertices, m_nVertices, field_C, field_10,
// m_pDrawHLineFunction.
constexpr size_t kVidPolySize = 0x18;

// CVidPalette::RealizeRange(pDestPalette, nBpp, dwFlags, pAffectArgs,
// nTransVal) (0x7BF870), `ret 0x14` at 0x7C04AC.
using RealizeRangeFn = void(__fastcall*)(void* self, void* edx, uint32_t* pDest, int32_t nBpp, uint32_t dwFlags,
    void* pAffectArgs, uint32_t nTransVal);
constexpr uintptr_t kRealizeRange = 0x7BF870;
// CVidPalette::CVidPalette(USHORT nType) (0x7BEEA0), `ret 4`.
using PaletteCtorFn = void*(__fastcall*)(void* self, void* edx, uint32_t nType);
constexpr uintptr_t kPaletteCtor = 0x7BEEA0;
// CVidPalette: m_nAUCounter 0, m_nAUCounterBase 4, m_pPalette 8, m_nEntries
// 0xC, rgbGlobalTint 0x10, m_nType 0x14, m_bPaletteOwner 0x16,
// m_bSubRangesCalculated 0x18, m_rangeColors 0x1C.
constexpr size_t kPaletteSize = 0x24;
constexpr size_t kPaletteBaseOffset = 0x4;
constexpr size_t kPalettePaletteOffset = 0x8;
constexpr size_t kPaletteSubRangesOffset = 0x18;
// CVidInf::CVidInf() (0x79AF00), plain ret; operator new(0x73E) at 0x7951DC.
using VidInfCtorFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kVidInfCtor = 0x79AF00;
constexpr size_t kVidInfSize = 0x73E;
// CVidMode members RealizeRange and its callees read.
constexpr size_t kFadeOffset = 0x49;
constexpr size_t kGlobalTintOffset = 0xA4;
constexpr size_t kBrightnessOffset = 0xA8;
constexpr size_t kGammaOffset = 0xA9;
constexpr size_t kRShiftOffset = 0xAA;
constexpr size_t kGShiftOffset = 0xAE;
constexpr size_t kBShiftOffset = 0xB2;
// g_pChitin and its members.
constexpr uintptr_t kChitin = 0x8CF6D8;
constexpr size_t kChitinSize = 0x2000;
constexpr size_t kActiveEngineOffset = 0x3C4;
constexpr size_t kIs3dOffset = 0x91C;
constexpr size_t kAUCounterOffset = 0x190E;
constexpr size_t kEngineSize = 0x10;
constexpr size_t kEngineVidModeOffset = 0x4;
// CVIDIMG_PALETTEAFFECT.
constexpr size_t kAffectSize = 0x78;

void* OriginalNew(size_t size)
{
    return reinterpret_cast<OriginalNewFn>(kOriginalNew)(size);
}

// A zeroed block from the original's operator new, leaked.
uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(OriginalNew(size));
    std::memset(at, 0, size);
    return at;
}

template <class T>
void Put(uint8_t* at, size_t offset, T value)
{
    std::memcpy(at + offset, &value, sizeof(value));
}

template <class T>
T Get(const uint8_t* at, size_t offset)
{
    T value;
    std::memcpy(&value, at + offset, sizeof(value));
    return value;
}

// Picks inputs from the run's --seed, apart from other streams by a salt.
struct Picks {
    Picks(uint64_t seed, uint64_t salt)
        : state(seed ^ salt)
        , bits(oracle::SplitMix64(&state))
    {
    }
    uint32_t Bits() { return *bits.Next(); }
    int32_t Between(int32_t lo, int32_t hi)
    {
        const uint32_t span = static_cast<uint32_t>(hi) - static_cast<uint32_t>(lo) + 1u;
        return static_cast<int32_t>(static_cast<uint32_t>(lo) + (span ? Bits() % span : Bits()));
    }

    uint64_t state;
    oracle::RandomGen<uint32_t> bits;
};

std::string Hex(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", value);
    return text;
}

// Records the first differing entry of two arrays.
void CompareArrays(CaseContext& ctx, const uint32_t* original, const uint32_t* ours, int count)
{
    for (int i = 0; i < count; ++i) {
        if (original[i] != ours[i]) {
            ctx.Fail(ctx.Inputs() + ", entry " + std::to_string(i), Hex(original[i]), Hex(ours[i]));
            return;
        }
    }
}

constexpr int kLineWidth = 64;

// One DrawHLineShaded32 (or the mirrored one) on a 64x1 row of random
// pixels: xMin and xMax anywhere in the row (including empty spans), the
// surface rectangle as wide as the row allows.
void HLineShaded(CaseContext& ctx, uintptr_t address, bool mirrored, uint64_t salt)
{
    uint8_t* poly = nullptr;
    uint32_t* row = nullptr;
    uint8_t* rect = nullptr;
    uint8_t* ref = nullptr;
    ctx.SetInputs("building the original's buffers");
    ctx.CallOriginal([&] {
        poly = OriginalBlock(kVidPolySize);
        row = reinterpret_cast<uint32_t*>(OriginalBlock(kLineWidth * 4));
        rect = OriginalBlock(16);
        ref = OriginalBlock(8);
    });
    CVidPoly ours;
    Picks picks(ctx.seed, salt);

    auto check = [&](int32_t xMin, int32_t xMax, uint32_t color, int32_t left, int32_t right) {
        uint32_t pixels[kLineWidth];
        for (uint32_t& pixel : pixels) {
            // Dark pixels (sum below 32) and bright ones both occur.
            pixel = picks.Bits() >> picks.Between(0, 8);
        }
        std::memcpy(row, pixels, sizeof(pixels));
        const int32_t r[4] = { left, 0, right, 1 };
        std::memcpy(rect, r, sizeof(r));
        const int32_t pt[2] = { picks.Between(0, 7), picks.Between(0, 7) };
        std::memcpy(ref, pt, sizeof(pt));
        ctx.SetInputs((mirrored ? "DrawHLineShadedMirrored32(" : "DrawHLineShaded32(") + std::to_string(xMin)
            + ", " + std::to_string(xMax) + ", " + Hex(color) + ", rSurface " + std::to_string(left) + ".."
            + std::to_string(right) + ")");
        ctx.CallOriginal([&] {
            reinterpret_cast<HLineFn>(address)(poly, nullptr, row, xMin, xMax, color, rect, ref);
        });
        const CRect rSurface(left, 0, right, 1);
        const CPoint ptRef(pt[0], pt[1]);
        if (mirrored) {
            ours.DrawHLineShadedMirrored32(pixels, xMin, xMax, color, rSurface, ptRef);
        } else {
            ours.DrawHLineShaded32(pixels, xMin, xMax, color, rSurface, ptRef);
        }
        ctx.CountInputs(1);
        CompareArrays(ctx, row, pixels, kLineWidth);
    };

    // Every span of the row once, with the widest surface rectangle.
    for (int32_t xMin = 0; xMin < kLineWidth; ++xMin) {
        for (int32_t xMax = xMin - 1; xMax < kLineWidth; ++xMax) {
            check(xMin, xMax, picks.Bits() & 0xFFFFFF, 0, kLineWidth - 1);
        }
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        const int32_t xMin = picks.Between(0, kLineWidth - 1);
        const int32_t xMax = picks.Between(xMin - 2, kLineWidth - 1);
        // Mirrored: the span runs from right - left - xMin down to
        // right - left - xMax, which stays in the row.
        const int32_t width = picks.Between(xMax < xMin ? xMin : xMax, kLineWidth - 1);
        const int32_t left = picks.Between(-100, 100);
        check(xMin, xMax, picks.Bits(), left, left + width);
    }
}

void HLineShaded32(CaseContext& ctx)
{
    HLineShaded(ctx, kDrawHLineShaded32, false, 0x484C696E65530000ull); // "HLineS"
}

void HLineShadedMirrored32(CaseContext& ctx)
{
    HLineShaded(ctx, kDrawHLineShadedMirrored32, true, 0x484C696E654D0000ull); // "HLineM"
}

// CVidMode::ApplyBrightnessContrast(rgb) (0x795E00), `ret 4` at 0x795F02:
// reads m_nBrightnessCorrection (0xA8) and m_nGammaCorrection (0xA9).
using BrightnessFn = uint32_t(__fastcall*)(void* self, void* edx, uint32_t rgb);
constexpr uintptr_t kApplyBrightnessContrast = 0x795E00;

// ApplyBrightnessContrast over every brightness and gamma byte with random
// colours (and black, which RealizeRange passes). The video mode is a
// CVidInf from the original's constructor.
void BrightnessContrast(CaseContext& ctx)
{
    uint8_t* mode = nullptr;
    ctx.SetInputs("building the original's video mode");
    ctx.CallOriginal([&] {
        mode = OriginalBlock(kVidInfSize);
        reinterpret_cast<VidInfCtorFn>(kVidInfCtor)(mode, nullptr);
    });
    CVidInf ours;
    Picks picks(ctx.seed, 0x4272696768740000ull); // "Bright"
    for (int brightness = 0; brightness < 256; ++brightness) {
        for (int gamma = 0; gamma < 256; ++gamma) {
            for (int i = 0; i < 4; ++i) {
                const uint32_t rgb = i == 0 ? 0u : (picks.Bits() & 0xFFFFFF);
                ctx.SetInputs("brightness " + std::to_string(brightness) + ", gamma " + std::to_string(gamma)
                    + ", rgb " + Hex(rgb));
                uint32_t original = 0;
                ctx.CallOriginal([&] {
                    Put<uint8_t>(mode, kBrightnessOffset, static_cast<uint8_t>(brightness));
                    Put<uint8_t>(mode, kGammaOffset, static_cast<uint8_t>(gamma));
                    original = reinterpret_cast<BrightnessFn>(kApplyBrightnessContrast)(mode, nullptr, rgb);
                });
                ours.m_nBrightnessCorrection = static_cast<BYTE>(brightness);
                ours.m_nGammaCorrection = static_cast<BYTE>(gamma);
                const uint32_t mine = ours.ApplyBrightnessContrast(rgb);
                ctx.CountInputs(1);
                if (original != mine) {
                    ctx.Fail(ctx.Inputs(), Hex(original), Hex(mine));
                }
            }
        }
    }
}

// The video mode both sides read.
struct ModeValues {
    uint8_t fade;
    uint32_t globalTint;
    uint8_t brightness;
    uint8_t gamma;
};

// Puts our g_pChitin back when the case ends.
struct ChitinRestore {
    CChitin* saved = g_pChitin;
    ~ChitinRestore() { g_pChitin = saved; }
};

// CVidPalette::RealizeRange (0x7BF870) at 32 bpp: a range palette of 256
// random entries, the seven ranges each with or without a tint, an add and a
// light (pulsing or steady), the suppress mask, the global tint, add and
// light flags, grey and the 0x2000000 shading, and the video mode's fade,
// brightness, gamma and global tint. All 256 destination entries are
// compared.
void RealizeRange(CaseContext& ctx)
{
    ChitinRestore restore;
    Picks picks(ctx.seed, 0x5265616C52616E00ull); // "RealRan"

    uint8_t* chitin = nullptr;
    uint8_t* engine = nullptr;
    uint8_t* mode = nullptr;
    ctx.SetInputs("building the original's chitin and video mode");
    ctx.CallOriginal([&] {
        chitin = OriginalBlock(kChitinSize);
        engine = OriginalBlock(kEngineSize);
        mode = OriginalBlock(kVidInfSize);
        reinterpret_cast<VidInfCtorFn>(kVidInfCtor)(mode, nullptr);
        Put<uint32_t>(engine, kEngineVidModeOffset, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mode)));
        Put<uint32_t>(chitin, kActiveEngineOffset, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(engine)));
        Put<uint32_t>(chitin, kIs3dOffset, 0);
        Put<uint32_t>(mode, kRShiftOffset, 16);
        Put<uint32_t>(mode, kGShiftOffset, 8);
        Put<uint32_t>(mode, kBShiftOffset, 0);
        *reinterpret_cast<uint32_t*>(kChitin) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(chitin));
    });

    auto* oursChitinStorage = static_cast<unsigned char*>(::operator new(sizeof(CChitin)));
    std::memset(oursChitinStorage, 0, sizeof(CChitin));
    auto* oursEngineStorage = static_cast<unsigned char*>(::operator new(sizeof(CWarp)));
    std::memset(oursEngineStorage, 0, sizeof(CWarp));
    auto* oursChitin = reinterpret_cast<CChitin*>(oursChitinStorage);
    auto* oursEngine = reinterpret_cast<CWarp*>(oursEngineStorage);
    auto* oursMode = new CVidInf();
    oursMode->m_dwRBitShift = 16;
    oursMode->m_dwGBitShift = 8;
    oursMode->m_dwBBitShift = 0;
    oursEngine->pVidMode = oursMode;
    oursChitin->pActiveEngine = oursEngine;
    g_pChitin = oursChitin;

    const uint32_t kFlagBits[] = { 0x1, 0x8, 0x10000, 0x20000, 0x40000, 0x80000, 0x200000, 0x2000000 };

    auto check = [&](int n) {
        ModeValues values;
        values.fade = static_cast<uint8_t>(picks.Between(0, 3) == 0 ? picks.Between(0, CVidMode::NUM_FADE_FRAMES)
                                                                   : CVidMode::NUM_FADE_FRAMES);
        values.globalTint = picks.Between(0, 2) == 0 ? 0xFFFFFF : (picks.Bits() & 0xFFFFFF);
        values.brightness = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 255) : 0);
        values.gamma = static_cast<uint8_t>(picks.Between(0, 2) == 0 ? picks.Between(0, 40) : 0);
        const int32_t auCounter = picks.Between(0, 100000);
        const int32_t auBase = picks.Between(1, 1 << 24);
        const uint32_t subRanges = static_cast<uint32_t>(picks.Between(0, 1));
        uint32_t dwFlags = 0;
        for (const uint32_t bit : kFlagBits) {
            if (picks.Between(0, 2) == 0) {
                dwFlags |= bit;
            }
        }
        const uint32_t nTransVal = picks.Between(0, 1) ? 0xFF000000u : 0u;

        RGBQUAD entries[256];
        for (RGBQUAD& entry : entries) {
            const uint32_t bits = picks.Bits();
            std::memcpy(&entry, &bits, sizeof(entry));
        }

        // The affect arguments: colours, then per range a tint, an add and
        // a light, each absent, steady (period 1; 0 also for the add) or
        // pulsing (period 2 to 30).
        uint32_t colors[3];
        for (uint32_t& color : colors) {
            color = picks.Bits() & 0xFFFFFF;
        }
        uint32_t rangeColors[3][7] = {};
        bool present[3][7] = {};
        uint8_t periods[3][7] = {};
        for (int kind = 0; kind < 3; ++kind) {
            for (int range = 0; range < 7; ++range) {
                present[kind][range] = picks.Between(0, 2) != 0;
                rangeColors[kind][range] = picks.Bits() & 0xFFFFFF;
                const int32_t form = picks.Between(0, 2);
                if (form == 0) {
                    periods[kind][range] = static_cast<uint8_t>(kind == 1 && picks.Between(0, 1) ? 0 : 1);
                } else {
                    periods[kind][range] = static_cast<uint8_t>(picks.Between(2, 30));
                }
            }
        }
        // A range without an add keeps the previous range's add percentage,
        // and a suppressed range then divides by its own add period: with a
        // period of 0 the original divides by zero (0x7BFEC1). Inputs with
        // an add period of 0 suppress nothing.
        bool zeroAddPeriod = false;
        for (int range = 0; range < 7; ++range) {
            zeroAddPeriod = zeroAddPeriod || periods[1][range] == 0;
        }
        const uint8_t suppress = zeroAddPeriod ? 0 : static_cast<uint8_t>(picks.Bits());

        char inputs[200];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: dwFlags 0x%X, nTransVal 0x%X, fade %u, tint 0x%X, brightness %u, gamma %u, AU %d+%d, "
            "subranges %u, suppress 0x%X",
            n, dwFlags, nTransVal, values.fade, values.globalTint, values.brightness, values.gamma, auBase,
            auCounter, subRanges, suppress);
        ctx.SetInputs(inputs);

        uint32_t* originalDest = nullptr;
        ctx.CallOriginal([&] {
            Put<uint8_t>(mode, kFadeOffset, values.fade);
            Put<uint32_t>(mode, kGlobalTintOffset, values.globalTint);
            Put<uint8_t>(mode, kBrightnessOffset, values.brightness);
            Put<uint8_t>(mode, kGammaOffset, values.gamma);
            Put<int32_t>(chitin, kAUCounterOffset, auCounter);

            uint8_t* palette = OriginalBlock(kPaletteSize);
            reinterpret_cast<PaletteCtorFn>(kPaletteCtor)(palette, nullptr, 1);
            Put<int32_t>(palette, kPaletteBaseOffset, auBase);
            Put<uint32_t>(palette, kPaletteSubRangesOffset, subRanges);
            auto* paletteEntries = reinterpret_cast<uint8_t*>(
                static_cast<uintptr_t>(Get<uint32_t>(palette, kPalettePaletteOffset)));
            std::memcpy(paletteEntries, entries, sizeof(entries));

            uint8_t* affect = OriginalBlock(kAffectSize);
            Put<uint32_t>(affect, 0x0, colors[0]);
            Put<uint32_t>(affect, 0x4, colors[1]);
            Put<uint32_t>(affect, 0x8, colors[2]);
            const size_t pointerOffsets[3] = { 0x0C, 0x30, 0x54 };
            const size_t periodOffsets[3] = { 0x28, 0x4C, 0x70 };
            uint8_t* targets = OriginalBlock(3 * 7 * 4);
            for (int kind = 0; kind < 3; ++kind) {
                for (int range = 0; range < 7; ++range) {
                    uint8_t* target = targets + (kind * 7 + range) * 4;
                    Put<uint32_t>(target, 0, rangeColors[kind][range]);
                    Put<uint32_t>(affect, pointerOffsets[kind] + range * 4,
                        present[kind][range] ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)) : 0u);
                    Put<uint8_t>(affect, periodOffsets[kind] + range, periods[kind][range]);
                }
            }
            Put<uint8_t>(affect, 0x77, suppress);

            originalDest = reinterpret_cast<uint32_t*>(OriginalBlock(256 * 4));
            reinterpret_cast<RealizeRangeFn>(kRealizeRange)(palette, nullptr, originalDest, 32, dwFlags, affect,
                nTransVal);
        });

        oursMode->m_nFade = values.fade;
        oursMode->rgbGlobalTint = values.globalTint;
        oursMode->m_nBrightnessCorrection = values.brightness;
        oursMode->m_nGammaCorrection = values.gamma;
        oursChitin->nAUCounter = auCounter;

        CVidPalette palette(CVidPalette::TYPE_RANGE);
        palette.m_nAUCounterBase = auBase;
        palette.m_bSubRangesCalculated = static_cast<BOOL>(subRanges);
        std::memcpy(palette.m_pPalette, entries, sizeof(entries));

        CVIDIMG_PALETTEAFFECT affect;
        std::memset(&affect, 0, sizeof(affect));
        affect.rgbTintColor = colors[0];
        affect.rgbAddColor = colors[1];
        affect.rgbLightColor = colors[2];
        COLORREF targets[3][7];
        for (int range = 0; range < 7; ++range) {
            for (int kind = 0; kind < 3; ++kind) {
                targets[kind][range] = rangeColors[kind][range];
            }
            affect.pRangeTints[range] = present[0][range] ? &targets[0][range] : nullptr;
            affect.aRangeTintPeriods[range] = periods[0][range];
            affect.pRangeAdds[range] = present[1][range] ? &targets[1][range] : nullptr;
            affect.aRangeAddPeriods[range] = periods[1][range];
            affect.pRangeLights[range] = present[2][range] ? &targets[2][range] : nullptr;
            affect.aRangeLightPeriods[range] = periods[2][range];
        }
        affect.suppressTints = suppress;

        DWORD oursDest[256] = {};
        palette.RealizeRange(oursDest, 32, dwFlags, &affect, nTransVal);
        ctx.CountInputs(1);
        CompareArrays(ctx, originalDest, oursDest, 256);
    };

    for (int i = 0; i < 64 + ctx.iterations; ++i) {
        check(i);
    }

    // oursMode is left alive: its destructor is not part of the case.
    ::operator delete(oursEngineStorage);
    ::operator delete(oursChitinStorage);
}

} // namespace

ORACLE_CASE("vid/hline_shaded32", HLineShaded32);
ORACLE_CASE("vid/hline_shaded_mirrored32", HLineShadedMirrored32);
ORACLE_CASE("vid/realize_range", RealizeRange);
ORACLE_CASE("vid/brightness_contrast", BrightnessContrast);
