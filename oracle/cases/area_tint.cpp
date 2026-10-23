// CGameArea::GetTintColor (0x470F10), the original's against ours. Rule-3
// probe for WSO row 0x470F10. Includes engine headers, so not <windows.h>
// (see spawn.cpp).
//
// The function reads the area (m_bmLum's pRes at +0x302, the night map
// pointer +0x318, m_cInfinity's m_areaType +0x624, m_renderDayNightCode
// +0x626 and m_dayLightIntensity +0x628) and, for a night code with no night
// map, the active engine's video mode's global tint (g_pBaldurChitin
// +0x3C4 -> engine +0x4 -> mode +0xA4). Both sides get the same random
// 24-bit light maps (parsed by CResBitmap::Parse on each side), a night map
// or none, and the same random points. The decisive inputs are a code with
// both bits (3) and no night map: the exe answers the global tint, the fork
// would interpolate the day corners. Code 0 is left out: the exe then
// interpolates uninitialised stack (ours zeroes it, a NOTE in the source).
// Points keep the cell and its right and lower neighbours inside the map.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CGameArea.h"
#include "CResBitmap.h"
#include "CVidBitmap.h"
#include "CVidMode.h"
#include "CWarp.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
// CResBitmap::Parse(void* pData) (0x77F1C0), `ret 4`.
using ParseFn = int32_t(__fastcall*)(void* self, void* edx, void* data);
constexpr uintptr_t kParse = 0x77F1C0;
// CGameArea::GetTintColor(const CPoint*, BYTE listType) (0x470F10), `ret 8`.
using TintFn = uint32_t(__fastcall*)(void* self, void* edx, const int32_t* point, uint32_t listType);
constexpr uintptr_t kGetTintColor = 0x470F10;
constexpr uintptr_t kGpBaldurChitin = 0x8CF6DC, kGpChitin = 0x8CF6D8;
constexpr size_t kAreaSize = 0x900, kResBitmapSize = 0x6C, kVidBitmapSize = 0x100;
constexpr size_t kLumPRes = 0x302, kLumNight = 0x318, kAreaType = 0x624, kCode = 0x626, kIntensity = 0x628;
constexpr size_t kChitinSize = 0x2000, kActiveEngine = 0x3C4, kEngineVidMode = 0x4, kGlobalTint = 0xA4;
constexpr size_t kBmpPRes = 0xA4;
constexpr int kW = 40, kH = 24; // light map pixels (cells 16x12 map units)

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}
template <class T>
void Put(uint8_t* at, size_t offset, T value) { std::memcpy(at + offset, &value, sizeof(value)); }
uint32_t Ptr32(const void* at) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(at)); }

// A bottom-up 24-bit BMP of kW x kH random pixels.
std::vector<uint8_t> MakeBmp(oracle::RandomGen<uint32_t>& bits)
{
    const int stride = (kW * 3 + 3) & ~3;
    std::vector<uint8_t> b(54 + static_cast<size_t>(stride) * kH);
    b[0] = 'B';
    b[1] = 'M';
    auto put32 = [&](size_t o, uint32_t v) { std::memcpy(&b[o], &v, 4); };
    put32(2, static_cast<uint32_t>(b.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, kW);
    put32(22, kH);
    b[26] = 1;
    b[28] = 24;
    put32(34, static_cast<uint32_t>(stride * kH));
    for (size_t i = 54; i < b.size(); i++) {
        b[i] = static_cast<uint8_t>(*bits.Next());
    }
    return b;
}

struct Side {
    uint8_t* original = nullptr;
    CGameArea* ours = nullptr;
};

// Builds both areas over the same bytes: a day map, and a night map when
// withNight.
Side Build(CaseContext& ctx, const std::vector<uint8_t>& day, const std::vector<uint8_t>& night, bool withNight,
    bool withDay, uint8_t areaType, uint8_t code, uint8_t intensity)
{
    Side s;
    ctx.CallOriginal([&] {
        auto res = [&](const std::vector<uint8_t>& bytes) {
            uint8_t* data = OriginalBlock(bytes.size());
            std::memcpy(data, bytes.data(), bytes.size());
            uint8_t* r = OriginalBlock(kResBitmapSize);
            if (!reinterpret_cast<ParseFn>(kParse)(r, nullptr, data)) {
                throw std::runtime_error("the original's CResBitmap::Parse refused the bitmap");
            }
            return r;
        };
        uint8_t* area = OriginalBlock(kAreaSize);
        if (withDay) {
            Put<uint32_t>(area, kLumPRes, Ptr32(res(day)));
        }
        if (withNight) {
            uint8_t* bmp = OriginalBlock(kVidBitmapSize);
            Put<uint32_t>(bmp, kBmpPRes, Ptr32(res(night)));
            Put<uint32_t>(area, kLumNight, Ptr32(bmp));
        }
        Put<uint16_t>(area, kAreaType, areaType);
        Put<uint8_t>(area, kCode, code);
        Put<uint8_t>(area, kIntensity, intensity);
        s.original = area;
    });
    auto oursRes = [&](const std::vector<uint8_t>& bytes) {
        auto* data = new uint8_t[bytes.size()];
        std::memcpy(data, bytes.data(), bytes.size());
        auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CResBitmap)));
        std::memset(storage, 0, sizeof(CResBitmap));
        auto* r = reinterpret_cast<CResBitmap*>(storage);
        if (!r->Parse(data)) {
            throw std::runtime_error("our CResBitmap::Parse refused the bitmap");
        }
        return r;
    };
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(storage, 0, sizeof(CGameArea));
    s.ours = reinterpret_cast<CGameArea*>(storage);
    // Our members are used by name: the header's offset comments are not the
    // exe's layout on this build (pRes at 720, not 0x302), and need not be.
    if (withDay) {
        s.ours->m_bmLum.pRes = oursRes(day);
    }
    if (withNight) {
        auto* bmp = static_cast<unsigned char*>(::operator new(sizeof(CVidBitmap)));
        std::memset(bmp, 0, sizeof(CVidBitmap));
        s.ours->m_pbmLumNight = reinterpret_cast<CVidBitmap*>(bmp);
        s.ours->m_pbmLumNight->pRes = oursRes(night);
    }
    s.ours->m_cInfinity.m_areaType = areaType;
    s.ours->m_cInfinity.m_renderDayNightCode = code;
    s.ours->m_cInfinity.m_dayLightIntensity = intensity;
    return s;
}

void TintCase(CaseContext& ctx)
{
    uint64_t state = ctx.seed ^ 0x7143C01044F10ull;
    oracle::RandomGen<uint32_t> bits(oracle::SplitMix64(&state));
    auto between = [&](int32_t lo, int32_t hi) {
        const uint32_t span = static_cast<uint32_t>(hi - lo + 1);
        return lo + static_cast<int32_t>(*bits.Next() % span);
    };

    // The active engine's video mode, global tint set per input.
    uint8_t* chitin = nullptr;
    uint8_t* mode = nullptr;
    ctx.SetInputs("building the original's chitin, engine and video mode");
    ctx.CallOriginal([&] {
        chitin = OriginalBlock(kChitinSize);
        uint8_t* engine = OriginalBlock(0x10);
        mode = OriginalBlock(0x100);
        Put<uint32_t>(engine, kEngineVidMode, Ptr32(mode));
        Put<uint32_t>(chitin, kActiveEngine, Ptr32(engine));
    });
    auto* oursChitin = reinterpret_cast<CBaldurChitin*>(OriginalBlock(sizeof(CBaldurChitin)));
    auto* oursEngine = reinterpret_cast<CBaldurEngine*>(OriginalBlock(sizeof(CBaldurEngine)));
    auto* oursMode = reinterpret_cast<CVidMode*>(OriginalBlock(sizeof(CVidMode)));
    oursEngine->pVidMode = oursMode;
    oursChitin->pActiveEngine = oursEngine;

    int inputs = 0;
    for (int i = 0; i < 24; i++) {
        const auto day = MakeBmp(bits);
        const auto night = MakeBmp(bits);
        for (int arm = 0; arm < 7; arm++) {
            // arm: code 1 day only, 2 night only, 3 both (with a night map);
            // 2 and 3 with no night map (the decisive ones); areaType without
            // 0x40 (code forced to 1); no day map.
            uint8_t code = 1;
            uint8_t areaType = 0x40;
            bool withNight = false, withDay = true;
            switch (arm) {
            case 0: code = 1; break;
            case 1: code = 2; withNight = true; break;
            case 2: code = 3; withNight = true; break;
            case 3: code = 2; break;
            case 4: code = 3; break;
            case 5: code = 3; areaType = 0x00; break;
            case 6: code = 3; withDay = false; break;
            }
            const uint8_t intensity = static_cast<uint8_t>(*bits.Next());
            const uint32_t tint = *bits.Next() & 0xFFFFFF;
            Side s = Build(ctx, day, night, withNight, withDay, areaType, code, intensity);
            for (int p = 0; p < 16; p++) {
                const int32_t x = between(0, (kW - 2) * 16 + 15), y = between(0, (kH - 2) * 12 + 11);
                const uint32_t listType = between(0, 9) == 0 ? 2 : between(0, 1);
                ctx.SetInputs("arm " + std::to_string(arm) + " code " + std::to_string(code) + " areaType "
                    + std::to_string(areaType) + " intensity " + std::to_string(intensity) + " tint "
                    + std::to_string(tint) + " point (" + std::to_string(x) + ", " + std::to_string(y)
                    + ") list " + std::to_string(listType));
                const int32_t pt[2] = { x, y };
                uint32_t original = 0;
                ctx.CallOriginal([&] {
                    Put<uint32_t>(mode, kGlobalTint, tint);
                    Put<uint32_t>(reinterpret_cast<uint8_t*>(kGpBaldurChitin), 0, Ptr32(chitin));
                    Put<uint32_t>(reinterpret_cast<uint8_t*>(kGpChitin), 0, Ptr32(chitin));
                    original = reinterpret_cast<TintFn>(kGetTintColor)(s.original, nullptr, pt, listType);
                });
                oursMode->rgbGlobalTint = tint;
                g_pBaldurChitin = oursChitin;
                const uint32_t ours = s.ours->GetTintColor(CPoint(x, y), static_cast<BYTE>(listType));
                g_pBaldurChitin = nullptr;
                ctx.CountInputs(1);
                inputs++;
                oracle::Compare(ctx, original, ours);
            }
        }
    }
}
} // namespace
ORACLE_CASE("area/get_tint_color", TintCase);
