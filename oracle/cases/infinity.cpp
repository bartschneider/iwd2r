// The area layer's WED/CInfinity fixture, shared by CTiledObject::CheckTileState
// (0x54E5B0) here and the CInfinity scrolling functions that follow.
//
// A real WED is built from the packed structs in src/BalDataTypes.h and parsed
// with each side's own CResWED::Parse, so the original's accessors (0x401220,
// 0x401270, 0x4012C0, 0x4012E0) read the same bytes the port reads. The
// CInfinity stands in with one CInfTileSet whose VRAM pool is a plain
// TileDefinition array: CheckTileState's swap path (CInfinity::SwapVRamTiles,
// 0x5D2350) only reads the pool entries and the tiles' m_nVRamTile and
// m_renderCode, so no surfaces are needed. The tile set is wired at each side's
// own layout: the original's objects are raw blocks at the 32-bit offsets from
// the disassembly (CInfinity pTileSets +0, pResWED +0x14; CInfTileSet pool +0xB0,
// res tiles +0xB4, count +0xB8; CVRamPool count +0, surfaces +4, defs +8;
// CResInfTile m_nVRamTile +0x64, m_renderCode +0x68; CTiledObject m_nWedIndex +0,
// m_pResWed +4, m_wAIState +8, m_wRenderState +0xA), ours through the objects'
// members.
//
// The case varies the tiled object's tile range, both poly counts and offsets,
// the poly type bytes, the tile data, the tile list, the render codes and the
// pool refcount, and exercises both tile states, the equal-state early out and
// the -1 WED index. It compares the WED image, the pool entry, every tile's
// render code and VRAM fields, and the object's state.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CChitin.h"
#include "CInfinity.h"
#include "CResWED.h"
#include "CTiledObject.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

void* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
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

// The fixture's WED layout. The offsets are fixed so the parts do not overlap;
// the case fills the entries.
constexpr int kWEDSize = 0x400;
constexpr int kLayerHeaderOffset = 0x40;
constexpr int kPolyHeaderOffset = 0x60;
constexpr int kTiledObjectsOffset = 0x80;
constexpr int kObjectTileListOffset = 0xC0;
constexpr int kTileDataOffset = 0x100;
constexpr int kTileListOffset = 0x380;
constexpr int kPolyListOffset = 0x390;
constexpr int kPolyPointsOffset = 0x3D8;
constexpr int kTilesAcross = 8;
constexpr int kTilesDown = 8;
constexpr int kTileSetSize = 8;
constexpr int kPolyCount = 4;
constexpr int kPolySize = sizeof(WED_POLYLIST);

// The original's 32-bit layouts (the disassembly of CheckTileState's callees).
constexpr size_t kResWedSize = 0x100;
constexpr size_t kInfinitySize = 0x1000;
constexpr size_t kInfinityTileSets = 0x00;
constexpr size_t kInfinityResWED = 0x14;
constexpr size_t kTileSetSizeBytes = 0xC0;
constexpr size_t kTileSetPool = 0xB0;
constexpr size_t kTileSetResTiles = 0xB4;
constexpr size_t kTileSetNumTiles = 0xB8;
constexpr size_t kVRamPoolSize = 0x10;
constexpr size_t kVRamPoolNumVTiles = 0x00;
constexpr size_t kVRamPoolTileDefs = 0x08;
constexpr size_t kTileDefinitionSize = 0xC;
constexpr size_t kTileDefinitionTile = 0x0;
constexpr size_t kTileDefinitionRefCount = 0x4;
constexpr size_t kResInfTileSize = 0x74;
constexpr size_t kResInfTileVRamTile = 0x64;
constexpr size_t kResInfTileRenderCode = 0x68;
constexpr size_t kResInfTileVRamFlags = 0x70;
constexpr size_t kTiledObjectSize = 0x14;
constexpr size_t kTiledObjectWedIndex = 0x0;
constexpr size_t kTiledObjectResWed = 0x4;
constexpr size_t kTiledObjectAIState = 0x8;
constexpr size_t kTiledObjectRenderState = 0xA;

// CResWED::Parse(void* pData) (0x401340): __thiscall, `ret 4`.
using ParseWedFn = int32_t(__fastcall*)(void* self, void* edx, void* pData);
constexpr uintptr_t kParseWed = 0x401340;
// CTiledObject::CheckTileState(CInfinity&) (0x54E5B0): __thiscall, `ret 4`.
using CheckTileStateFn = void(__fastcall*)(void* self, void* edx, void* cInfinity);
constexpr uintptr_t kCheckTileState = 0x54E5B0;

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

std::string Hex16(uint16_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%04X", value);
    return text;
}

void Put16(std::vector<uint8_t>& image, int at, uint16_t value)
{
    std::memcpy(image.data() + at, &value, sizeof(value));
}

void Put32(std::vector<uint8_t>& image, int at, uint32_t value)
{
    std::memcpy(image.data() + at, &value, sizeof(value));
}

// The fixture's image at its fixed layout: a valid header, one 8x8 layer, one
// tiled object and a four-poly poly list. The case fills the varying entries.
void BuildEmptyImage(std::vector<uint8_t>& image)
{
    image.assign(kWEDSize, 0);
    std::memcpy(image.data(), "WED ", 4);
    std::memcpy(image.data() + 4, "V1.3", 4);
    Put32(image, 0x08, 1); // nLayers
    Put32(image, 0x0C, 1); // nTiledObjects
    Put32(image, 0x10, kLayerHeaderOffset);
    Put32(image, 0x14, kPolyHeaderOffset);
    Put32(image, 0x18, kTiledObjectsOffset);
    Put32(image, 0x1C, kObjectTileListOffset);

    Put16(image, kLayerHeaderOffset + 0x00, kTilesAcross);
    Put16(image, kLayerHeaderOffset + 0x02, kTilesDown);
    Put16(image, kLayerHeaderOffset + 0x0C, kTileSetSize);
    Put32(image, kLayerHeaderOffset + 0x10, kTileDataOffset);
    Put32(image, kLayerHeaderOffset + 0x14, kTileListOffset);

    Put32(image, kPolyHeaderOffset + 0x00, kPolyCount);
    Put32(image, kPolyHeaderOffset + 0x04, kPolyListOffset);
    Put32(image, kPolyHeaderOffset + 0x08, kPolyPointsOffset);
}

void CompareArrays(CaseContext& ctx, const uint32_t* original, const uint32_t* ours, int count)
{
    for (int i = 0; i < count; ++i) {
        if (original[i] != ours[i]) {
            ctx.Fail(ctx.Inputs() + ", word " + std::to_string(i), Hex(original[i]), Hex(ours[i]));
            return;
        }
    }
}

// CTiledObject::CheckTileState (0x54E5B0): the tiled object's tiles take the AI
// state's wFlags and swap their VRAM images (primary <-> secondary), and its
// primary/secondary polys clear/set the WED_POLYLIST type byte's bit 1 (the
// reverse for the secondary state).
void CheckTileState(CaseContext& ctx)
{
    uint8_t* resWed = nullptr;
    uint8_t* infinity = nullptr;
    uint8_t* tileSet = nullptr;
    uint8_t* pool = nullptr;
    uint8_t* tiles = nullptr;
    uint8_t* tilePointers = nullptr;
    uint8_t* defs = nullptr;
    uint8_t* object = nullptr;
    uint8_t* wedData = nullptr;
    ctx.SetInputs("building the original's blocks");
    ctx.CallOriginal([&] {
        resWed = static_cast<uint8_t*>(OriginalBlock(kResWedSize));
        infinity = static_cast<uint8_t*>(OriginalBlock(kInfinitySize));
        tileSet = static_cast<uint8_t*>(OriginalBlock(kTileSetSizeBytes));
        pool = static_cast<uint8_t*>(OriginalBlock(kVRamPoolSize));
        tiles = static_cast<uint8_t*>(OriginalBlock(kTileSetSize * kResInfTileSize));
        tilePointers = static_cast<uint8_t*>(OriginalBlock(kTileSetSize * 4));
        defs = static_cast<uint8_t*>(OriginalBlock(kTileDefinitionSize));
        object = static_cast<uint8_t*>(OriginalBlock(kTiledObjectSize));
        wedData = static_cast<uint8_t*>(OriginalBlock(kWEDSize));
    });

    std::vector<uint8_t> oursResWed(kResWedSize, 0);
    std::vector<uint8_t> oursInfinity(sizeof(CInfinity), 0);
    std::vector<uint8_t> oursTileSet(sizeof(CInfTileSet), 0);
    std::vector<uint8_t> oursPool(sizeof(CVRamPool), 0);
    std::vector<uint8_t> oursTiles(kTileSetSize * sizeof(CResInfTile), 0);
    std::vector<CResInfTile*> oursTilePointers(kTileSetSize, nullptr);
    TileDefinition oursDef = {};
    std::vector<uint8_t> oursObject(sizeof(CTiledObject), 0);
    std::vector<uint8_t> image;
    std::vector<uint8_t> oursImage;

    auto* ourResWed = reinterpret_cast<CResWED*>(oursResWed.data());
    auto* ourInfinity = reinterpret_cast<CInfinity*>(oursInfinity.data());
    auto* ourTileSet = reinterpret_cast<CInfTileSet*>(oursTileSet.data());
    auto* ourPool = reinterpret_cast<CVRamPool*>(oursPool.data());
    auto* ourObject = reinterpret_cast<CTiledObject*>(oursObject.data());
    for (int i = 0; i < kTileSetSize; i++) {
        oursTilePointers[i] = reinterpret_cast<CResInfTile*>(oursTiles.data() + i * sizeof(CResInfTile));
    }

    Picks picks(ctx.seed, 0x436865636B547300ull); // "CheckTs"
    const int count = 500 + ctx.iterations / 20;
    for (int n = 0; n < count; ++n) {
        BuildEmptyImage(image);

        const int32_t startingTile = picks.Between(0, 2);
        const int32_t numTiles = picks.Between(1, 4);
        const uint16_t numPrimary = static_cast<uint16_t>(picks.Between(0, 3) == 0 ? (picks.Between(0, 1) ? 0 : 0xFFFF) : picks.Between(0, 2));
        const uint16_t numSecondary = static_cast<uint16_t>(picks.Between(0, 3) == 0 ? (picks.Between(0, 1) ? 0 : 0xFFFF) : picks.Between(0, 2));
        const int32_t primaryPoly = picks.Between(0, 2);
        const int32_t secondaryPoly = picks.Between(0, 2);
        const uint16_t wedIndex = static_cast<uint16_t>(picks.Between(0, 2) == 0 ? 0xFFFF : 0);
        const uint16_t aiState = static_cast<uint16_t>(picks.Between(0, 1) == 0 ? 1 : 2);
        const uint16_t renderState = picks.Between(0, 4) == 0 ? aiState : static_cast<uint16_t>(aiState == 1 ? 2 : 1);
        // Above the number of swaps one input can make (the object tiles may
        // repeat the same image), so DetachFromVRam never empties the pool
        // entry: EmptyTile/AssociateTile would need real surfaces and are not
        // part of CheckTileState's contract here.
        const uint16_t refCount = static_cast<uint16_t>(numTiles + picks.Between(4, 8));

        // The tiled object and the object tile list.
        Put16(image, kTiledObjectsOffset + 0x0A, static_cast<uint16_t>(startingTile));
        Put16(image, kTiledObjectsOffset + 0x0C, static_cast<uint16_t>(numTiles));
        Put16(image, kTiledObjectsOffset + 0x0E, numPrimary);
        Put16(image, kTiledObjectsOffset + 0x10, numSecondary);
        Put32(image, kTiledObjectsOffset + 0x12, static_cast<uint32_t>(kPolyListOffset + primaryPoly * kPolySize));
        Put32(image, kTiledObjectsOffset + 0x16, static_cast<uint32_t>(kPolyListOffset + secondaryPoly * kPolySize));
        for (int i = 0; i < startingTile + numTiles; i++) {
            Put16(image, kObjectTileListOffset + 2 * i, static_cast<uint16_t>(picks.Between(0, 63)));
        }

        // The tile data grid.
        std::vector<uint16_t> tileStarting(kTilesAcross * kTilesDown);
        std::vector<uint16_t> tileSecondary(kTilesAcross * kTilesDown);
        for (int i = 0; i < kTilesAcross * kTilesDown; i++) {
            tileStarting[i] = static_cast<uint16_t>(picks.Between(0, kTileSetSize - 1));
            tileSecondary[i] = static_cast<uint16_t>(picks.Between(0, kTileSetSize - 1));
            auto* pTileData = reinterpret_cast<WED_TILEDATA*>(image.data() + kTileDataOffset + i * sizeof(WED_TILEDATA));
            pTileData->nStartingTile = tileStarting[i];
            pTileData->nNumTiles = static_cast<uint16_t>(picks.Between(1, 2));
            pTileData->nSecondary = tileSecondary[i];
            pTileData->bFlags = static_cast<uint8_t>(picks.Bits());
            pTileData->wFlags = static_cast<uint16_t>(picks.Bits());
        }

        // The layer tile list and the poly type bytes.
        std::vector<uint16_t> tileListValues(kTileSetSize);
        for (int i = 0; i < kTileSetSize; i++) {
            tileListValues[i] = static_cast<uint16_t>(picks.Between(0, kTileSetSize - 1));
            Put16(image, kTileListOffset + 2 * i, tileListValues[i]);
        }
        std::vector<uint8_t> polyTypes(kPolyCount);
        for (int i = 0; i < kPolyCount; i++) {
            polyTypes[i] = static_cast<uint8_t>(picks.Bits());
            image[kPolyListOffset + i * kPolySize + 8] = polyTypes[i];
        }

        // The render codes of the tile images.
        std::vector<uint8_t> renderCodes(kTileSetSize);
        for (int i = 0; i < kTileSetSize; i++) {
            renderCodes[i] = static_cast<uint8_t>(0x10 + i * 3);
        }

        char inputs[400];
        int used = std::snprintf(inputs, sizeof(inputs),
            "input %d: %dx%d, polys %u/%u at %d/%d, wed %u, state %u->%u, refs %u, tiles ", n,
            startingTile, numTiles, numPrimary, numSecondary, primaryPoly, secondaryPoly, wedIndex,
            renderState, aiState, refCount);
        for (int i = 0; i < startingTile + numTiles && used < static_cast<int>(sizeof(inputs)); i++) {
            const int index = Get<uint16_t>(image.data(), kObjectTileListOffset + 2 * i);
            used += std::snprintf(inputs + used, sizeof(inputs) - used, "%d:%d/%d ", index,
                image[kTileDataOffset + index * sizeof(WED_TILEDATA) + 0]
                    | (image[kTileDataOffset + index * sizeof(WED_TILEDATA) + 1] << 8),
                image[kTileDataOffset + index * sizeof(WED_TILEDATA) + 4]
                    | (image[kTileDataOffset + index * sizeof(WED_TILEDATA) + 5] << 8));
        }
        ctx.SetInputs(inputs);

        oursImage = image;

        ctx.CallOriginal([&] {
            std::memset(resWed, 0, kResWedSize);
            std::memset(infinity, 0, kInfinitySize);
            std::memset(tileSet, 0, kTileSetSizeBytes);
            std::memset(pool, 0, kVRamPoolSize);
            std::memset(tiles, 0, kTileSetSize * kResInfTileSize);
            std::memset(tilePointers, 0, kTileSetSize * 4);
            std::memset(defs, 0, kTileDefinitionSize);
            std::memset(object, 0, kTiledObjectSize);

            std::memcpy(wedData, image.data(), kWEDSize);
            reinterpret_cast<ParseWedFn>(kParseWed)(resWed, nullptr, wedData);

            for (int i = 0; i < kTileSetSize; i++) {
                Put<uint32_t>(tilePointers, i * 4, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tiles + i * kResInfTileSize)));
                uint8_t* pTile = tiles + i * kResInfTileSize;
                Put<int32_t>(pTile, kResInfTileVRamTile, 0);
                Put<uint8_t>(pTile, kResInfTileRenderCode + 0, renderCodes[i]);
                Put<uint8_t>(pTile, kResInfTileRenderCode + 1, static_cast<uint8_t>(renderCodes[i] + 1));
                Put<uint8_t>(pTile, kResInfTileRenderCode + 2, static_cast<uint8_t>(renderCodes[i] + 2));
                Put<uint8_t>(pTile, kResInfTileRenderCode + 3, static_cast<uint8_t>(renderCodes[i] + 3));
            }

            Put<uint32_t>(defs, kTileDefinitionTile, 0);
            Put<uint32_t>(defs, kTileDefinitionRefCount, refCount);
            Put<uint32_t>(defs, kTileDefinitionSize - 4, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tileSet)));
            Put<int32_t>(pool, kVRamPoolNumVTiles, 1);
            Put<uint32_t>(pool, kVRamPoolTileDefs, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(defs)));
            Put<uint32_t>(tileSet, kTileSetPool, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pool)));
            Put<uint32_t>(tileSet, kTileSetResTiles, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tilePointers)));
            Put<int32_t>(tileSet, kTileSetNumTiles, kTileSetSize);

            Put<uint32_t>(infinity, kInfinityTileSets, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tileSet)));
            Put<uint32_t>(infinity, kInfinityResWED, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(resWed)));

            Put<int32_t>(object, kTiledObjectWedIndex, wedIndex == 0xFFFF ? -1 : 0);
            Put<uint32_t>(object, kTiledObjectResWed, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(resWed)));
            Put<uint16_t>(object, kTiledObjectAIState, aiState);
            Put<uint16_t>(object, kTiledObjectRenderState, renderState);

            reinterpret_cast<CheckTileStateFn>(kCheckTileState)(object, nullptr, infinity);
        });

        std::memset(oursResWed.data(), 0, kResWedSize);
        std::memset(oursInfinity.data(), 0, oursInfinity.size());
        std::memset(oursTileSet.data(), 0, oursTileSet.size());
        std::memset(oursPool.data(), 0, oursPool.size());
        std::memset(oursTiles.data(), 0, oursTiles.size());
        std::memset(oursObject.data(), 0, oursObject.size());

        oursImage = image;
        if (!ourResWed->Parse(oursImage.data())) {
            ctx.Fail(ctx.Inputs() + ", ours: WED parse failed", "1", "0");
            continue;
        }

        for (int i = 0; i < kTileSetSize; i++) {
            auto* pTile = oursTilePointers[i];
            pTile->m_nVRamTile = 0;
            pTile->m_renderCode.tileNW = renderCodes[i];
            pTile->m_renderCode.tileNE = static_cast<uint8_t>(renderCodes[i] + 1);
            pTile->m_renderCode.tileSW = static_cast<uint8_t>(renderCodes[i] + 2);
            pTile->m_renderCode.tileSE = static_cast<uint8_t>(renderCodes[i] + 3);
            pTile->m_pDualTileRes = NULL;
            pTile->m_nVRamFlags = 0;
            ourTileSet->m_pResTiles = oursTilePointers.data();
        }

        oursDef.nTile = 0;
        oursDef.nRefCount = refCount;
        oursDef.pTileSet = ourTileSet;
        ourPool->m_nVTiles = 1;
        ourPool->m_pTileDefs = &oursDef;
        ourTileSet->m_pVRamPool = ourPool;
        ourTileSet->m_nTiles = kTileSetSize;
        ourInfinity->pTileSets[0] = ourTileSet;
        ourInfinity->pResWED = ourResWed;
        ourObject->m_pResWed = ourResWed;
        ourObject->m_nWedIndex = wedIndex == 0xFFFF ? -1 : 0;
        ourObject->m_wAIState = aiState;
        ourObject->m_wRenderState = renderState;

        ourObject->CheckTileState(*ourInfinity);
        ctx.CountInputs(1);

        CompareArrays(ctx, reinterpret_cast<const uint32_t*>(wedData), reinterpret_cast<const uint32_t*>(oursImage.data()),
            kWEDSize / 4);

        // The object's state, the pool entry and every tile's VRAM fields.
        const uint16_t originalRender = Get<uint16_t>(object, kTiledObjectRenderState);
        if (Get<int32_t>(object, kTiledObjectWedIndex) != static_cast<int32_t>(ourObject->m_nWedIndex)
            || Get<uint16_t>(object, kTiledObjectAIState) != ourObject->m_wAIState
            || originalRender != ourObject->m_wRenderState) {
            ctx.Fail(ctx.Inputs() + ", object state", Hex16(originalRender), Hex16(ourObject->m_wRenderState));
        }

        if (Get<int32_t>(defs, kTileDefinitionTile) != oursDef.nTile
            || Get<int32_t>(defs, kTileDefinitionRefCount) != oursDef.nRefCount
            || Get<int32_t>(pool, kVRamPoolNumVTiles) != ourPool->m_nVTiles) {
            ctx.Fail(ctx.Inputs() + ", pool", Hex(static_cast<uint32_t>(Get<int32_t>(defs, kTileDefinitionRefCount))),
                Hex(static_cast<uint32_t>(oursDef.nRefCount)));
        }

        for (int i = 0; i < kTileSetSize; i++) {
            const uint8_t* pTile = tiles + i * kResInfTileSize;
            const CResInfTile* pOurs = oursTilePointers[i];
            const uint8_t oursCode[4] = { pOurs->m_renderCode.tileNW, pOurs->m_renderCode.tileNE,
                pOurs->m_renderCode.tileSW, pOurs->m_renderCode.tileSE };
            char originalDetail[64];
            char oursDetail[64];
            std::snprintf(originalDetail, sizeof(originalDetail), "nvram %d code %02X%02X%02X%02X flags %02X",
                Get<int32_t>(pTile, kResInfTileVRamTile), pTile[kResInfTileRenderCode],
                pTile[kResInfTileRenderCode + 1], pTile[kResInfTileRenderCode + 2], pTile[kResInfTileRenderCode + 3],
                Get<uint8_t>(pTile, kResInfTileVRamFlags));
            std::snprintf(oursDetail, sizeof(oursDetail), "nvram %d code %02X%02X%02X%02X flags %02X",
                pOurs->m_nVRamTile, oursCode[0], oursCode[1], oursCode[2], oursCode[3], pOurs->m_nVRamFlags);
            if (Get<int32_t>(pTile, kResInfTileVRamTile) != pOurs->m_nVRamTile
                || std::memcmp(pTile + kResInfTileRenderCode, oursCode, 4) != 0
                || Get<uint8_t>(pTile, kResInfTileVRamFlags) != pOurs->m_nVRamFlags) {
                ctx.Fail(ctx.Inputs() + ", tile " + std::to_string(i), originalDetail, oursDetail);
            }
        }
    }
}

}

// CInfinity::ScrollingRequestRect(int x1, int y1, int x2, int y2, INT
// nScrollState) (0x5D00C0), `ret 0x14` at 0x5D078D. Its observable work is the
// RequestTile calls it makes, so the fixture maps every grid cell to its own
// CResInfTile and compares the request count (the CRes's field_44) and the
// last priority (dwFlags & 3) of all 64 tiles. RequestTile reaches them through
// CRes::SetPriority/Request (0x77E670/0x77E610) and g_pChitin->cDimm
// (CDimm::SetNewPriority/Request, 0x788BE0/0x787F20), so the original needs a
// chitin at 0x8CF6D8 whose field_2FC is a real kernel32 critical section (that
// is InitializeOriginalCriticalSection in the runner) while ours is a zeroed
// CChitin with the engine's shim section. The fixture's CResInfTile blocks are
// zeroed but for the request count and flags, and their m_pDualTileRes is NULL.
//
// The cell's tile is the *secondary* one (wFlags 2, nSecondary = cell index):
// RequestTile then requests it in its first block and again in its last one, so
// a visited cell gets two requests. The primary tile-list branch is not
// exercised here.
constexpr int kScrollTiles = kTilesAcross * kTilesDown;
constexpr size_t kInfinityTilesX = 0x38;
constexpr size_t kInfinityTilesY = 0x3C;
constexpr size_t kInfinityField68 = 0x68;
constexpr size_t kInfinityField6C = 0x6C;
constexpr size_t kInfinityField70 = 0x70;
constexpr size_t kInfinityField74 = 0x74;
constexpr size_t kInfinityRainLevel = 0x128;
constexpr size_t kInfinityAreaType = 0x158;
constexpr size_t kInfinityRequestDayNightCode = 0x15D;
constexpr size_t kInfinityField15E = 0x15E;
constexpr size_t kInfinityField196 = 0x196;
constexpr size_t kResFlags = 0x4;
constexpr size_t kResField44 = 0x44;

constexpr uintptr_t kOriginalChitinGlobal = 0x8CF6D8;
constexpr size_t kOriginalChitinSize = 0x2000;
constexpr size_t kOriginalChitinCriticalSection = 0x2FC;

// CDimm::CDimm (0x781E60): __thiscall, no arguments. The original's CDimm the
// request path enters is at chitin + 0x542; its constructor is what gives its
// request lists their MFC fixed-allocator state (a zeroed CObList crashes the
// original's AddTail).
using DimmCtorFn = void(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kDimmCtor = 0x781E60;

// CInfinity::ScrollingRequestRect: __thiscall, `ret 0x14`. The original's
// CDimm the request path enters is at chitin + 0x542.
using ScrollingRequestRectFn = int32_t(__fastcall*)(void* self, void* edx, int32_t x1, int32_t y1,
    int32_t x2, int32_t y2, int32_t nScrollState);
constexpr uintptr_t kScrollingRequestRect = 0x5D00C0;

// The scroll fixture's WED: the 8x8 tile grid with an identity tile list (a
// tile-list entry of 64 entries fits exactly in the 0x380..0x400 tail; the
// unused poly header has no polys).
void BuildScrollImage(std::vector<uint8_t>& image)
{
    BuildEmptyImage(image);
    Put32(image, 0x0C, 0); // nTiledObjects
    Put32(image, kPolyHeaderOffset + 0x00, 0);

    for (int i = 0; i < kScrollTiles; i++) {
        Put16(image, kTileListOffset + 2 * i, static_cast<uint16_t>(i));

        auto* pTileData = reinterpret_cast<WED_TILEDATA*>(image.data() + kTileDataOffset + i * sizeof(WED_TILEDATA));
        pTileData->nStartingTile = 0;
        pTileData->nNumTiles = 1;
        pTileData->nSecondary = static_cast<uint16_t>(i);
        // bFlags 0x02 names a second layer the fixture does not have, so the
        // layer walk is a no-op on both sides and the trailing secondary
        // request runs (RequestTile reaches it only after the layer block).
        pTileData->bFlags = 0x2;
        pTileData->wFlags = 2; // CTiledObject::STATE_SECONDARY_TILE
    }
}

// ScrollingRequestRect over an 8x8 map: random rects (including ones outside
// the map, which the clamps cut), scroll states 1-8, all four combinations of
// the day/night and rain area bits, a random day/night code and rain level, a
// random previous rect (cancelled first; field_196 stays 0 so the unported
// ScrollingCancelRequestRect is not reached) and random request counts on the
// tiles before the call. Compares the WED, the rect/flags/state fields and all
// 64 tiles' request count and flags.
void ScrollingRequestRect(CaseContext& ctx)
{
    uint8_t* wedData = nullptr;
    uint8_t* resWed = nullptr;
    uint8_t* infinity = nullptr;
    uint8_t* tileSet = nullptr;
    uint8_t* tilePointers = nullptr;
    uint8_t* tiles = nullptr;
    ctx.SetInputs("building the original's blocks");
    ctx.CallOriginal([&] {
        wedData = static_cast<uint8_t*>(OriginalBlock(kWEDSize));
        resWed = static_cast<uint8_t*>(OriginalBlock(kResWedSize));
        infinity = static_cast<uint8_t*>(OriginalBlock(kInfinitySize));
        tileSet = static_cast<uint8_t*>(OriginalBlock(kTileSetSizeBytes));
        tilePointers = static_cast<uint8_t*>(OriginalBlock(kScrollTiles * 4));
        tiles = static_cast<uint8_t*>(OriginalBlock(kScrollTiles * kResInfTileSize));
    });

    std::vector<uint8_t> oursResWed(kResWedSize, 0);
    std::vector<uint8_t> oursInfinity(sizeof(CInfinity), 0);
    std::vector<uint8_t> oursTileSet(sizeof(CInfTileSet), 0);
    std::vector<uint8_t> oursTiles(kScrollTiles * sizeof(CResInfTile), 0);
    std::vector<CResInfTile*> oursTilePointers(kScrollTiles, nullptr);
    std::vector<uint8_t> image;
    std::vector<uint8_t> oursImage;

    auto* ourResWed = reinterpret_cast<CResWED*>(oursResWed.data());
    auto* ourInfinity = reinterpret_cast<CInfinity*>(oursInfinity.data());
    auto* ourTileSet = reinterpret_cast<CInfTileSet*>(oursTileSet.data());
    for (int i = 0; i < kScrollTiles; i++) {
        oursTilePointers[i] = reinterpret_cast<CResInfTile*>(oursTiles.data() + i * sizeof(CResInfTile));
    }

    Picks picks(ctx.seed, 0x5363726F6C6C5200ull); // "ScrollR"
    const int count = 500 + ctx.iterations / 25;
    for (int n = 0; n < count; ++n) {
        BuildScrollImage(image);

        const int32_t x1 = picks.Between(-3, 12);
        const int32_t y1 = picks.Between(-3, 12);
        const int32_t x2 = picks.Between(-3, 12);
        const int32_t y2 = picks.Between(-3, 12);
        const int32_t nScrollState = picks.Between(1, 8);
        const uint16_t areaTypes[4] = { 0, 0x40, 0x04, 0x44 };
        const uint16_t areaType = areaTypes[picks.Between(0, 3)];
        const uint8_t dayNightCode = static_cast<uint8_t>(picks.Between(0, 255));
        const int32_t rainLevel = picks.Between(0, 3);
        const uint8_t oldFlags = static_cast<uint8_t>(picks.Between(0, 255));
        const bool bOldRect = picks.Between(0, 2) != 0;
        const int32_t oldX1 = picks.Between(-3, 10);
        const int32_t oldY1 = picks.Between(-3, 10);
        const int32_t oldX2 = picks.Between(-3, 10);
        const int32_t oldY2 = picks.Between(-3, 10);

        std::vector<uint8_t> requestCounts(kScrollTiles);
        for (int i = 0; i < kScrollTiles; i++) {
            requestCounts[i] = static_cast<uint8_t>(picks.Between(0, 2));
        }

        char inputs[240];
        std::snprintf(inputs, sizeof(inputs),
            "input %d: rect %d,%d-%d,%d state %d area 0x%02X day %u rain %d old 0x%02X %s%d,%d-%d,%d",
            n, x1, y1, x2, y2, nScrollState, areaType, dayNightCode, rainLevel, oldFlags,
            bOldRect ? "" : "none ", oldX1, oldY1, oldX2, oldY2);
        ctx.SetInputs(inputs);

        oursImage = image;

        ctx.CallOriginal([&] {
            std::memset(resWed, 0, kResWedSize);
            std::memset(infinity, 0, kInfinitySize);
            std::memset(tileSet, 0, kTileSetSizeBytes);
            std::memset(tilePointers, 0, kScrollTiles * 4);
            std::memset(tiles, 0, kScrollTiles * kResInfTileSize);

            std::memcpy(wedData, image.data(), kWEDSize);
            reinterpret_cast<ParseWedFn>(kParseWed)(resWed, nullptr, wedData);

            for (int i = 0; i < kScrollTiles; i++) {
                uint8_t* pTile = tiles + i * kResInfTileSize;
                Put<uint32_t>(tilePointers, i * 4, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pTile)));
                Put<int32_t>(pTile, kResField44, requestCounts[i]);
            }

            Put<uint32_t>(tileSet, kTileSetResTiles, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tilePointers)));
            Put<int32_t>(tileSet, kTileSetNumTiles, kScrollTiles);

            Put<uint32_t>(infinity, kInfinityTileSets, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tileSet)));
            Put<uint32_t>(infinity, kInfinityResWED, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(resWed)));
            Put<int32_t>(infinity, kInfinityTilesX, kTilesAcross);
            Put<int32_t>(infinity, kInfinityTilesY, kTilesDown);
            Put<uint16_t>(infinity, kInfinityAreaType, areaType);
            Put<uint8_t>(infinity, kInfinityRequestDayNightCode, dayNightCode);
            Put<uint8_t>(infinity, kInfinityField15E, oldFlags);
            Put<int32_t>(infinity, kInfinityField196, 0);
            if (bOldRect) {
                Put<int32_t>(infinity, kInfinityField68, oldX1);
                Put<int32_t>(infinity, kInfinityField6C, oldY1);
                Put<int32_t>(infinity, kInfinityField70, oldX2);
                Put<int32_t>(infinity, kInfinityField74, oldY2);
            } else {
                Put<int32_t>(infinity, kInfinityField68, -1);
                Put<int32_t>(infinity, kInfinityField6C, -1);
                Put<int32_t>(infinity, kInfinityField70, -1);
                Put<int32_t>(infinity, kInfinityField74, -1);
            }
            Put<int32_t>(infinity, kInfinityRainLevel, rainLevel);

            // The chitin, its CDimm and its critical section: the original's
            // SetPriority/Request enter the CDimm's MFC lists and kernel32's
            // critical section, so both need their real constructors.
            uint8_t* chitin = static_cast<uint8_t*>(OriginalBlock(kOriginalChitinSize));
            reinterpret_cast<DimmCtorFn>(kDimmCtor)(chitin + 0x542, nullptr);
            InitializeOriginalCriticalSection(chitin + kOriginalChitinCriticalSection);
            *reinterpret_cast<uint32_t*>(kOriginalChitinGlobal) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(chitin));

            reinterpret_cast<ScrollingRequestRectFn>(kScrollingRequestRect)(
                infinity, nullptr, x1, y1, x2, y2, nScrollState);
        });

        std::memset(oursResWed.data(), 0, kResWedSize);
        std::memset(oursInfinity.data(), 0, oursInfinity.size());
        std::memset(oursTileSet.data(), 0, oursTileSet.size());
        std::memset(oursTiles.data(), 0, oursTiles.size());

        if (!ourResWed->Parse(oursImage.data())) {
            ctx.Fail(ctx.Inputs() + ", ours: WED parse failed", "1", "0");
            continue;
        }

        std::vector<uint8_t> oursChitinStorage(sizeof(CChitin), 0);
        auto* ourChitin = reinterpret_cast<CChitin*>(oursChitinStorage.data());
        InitializeCriticalSection(&ourChitin->field_2FC);
        g_pChitin = ourChitin;

        for (int i = 0; i < kScrollTiles; i++) {
            auto* pTile = oursTilePointers[i];
            pTile->field_44 = requestCounts[i];
            pTile->field_40 = 0;
            pTile->dwFlags = 0;
            pTile->m_pDualTileRes = NULL;
        }

        ourTileSet->m_pVRamPool = NULL;
        ourTileSet->m_pResTiles = oursTilePointers.data();
        ourTileSet->m_nTiles = kScrollTiles;
        ourInfinity->pTileSets[0] = ourTileSet;
        ourInfinity->pResWED = ourResWed;
        ourInfinity->nTilesX = kTilesAcross;
        ourInfinity->nTilesY = kTilesDown;
        ourInfinity->m_areaType = areaType;
        ourInfinity->m_requestDayNightCode = dayNightCode;
        ourInfinity->field_15E = oldFlags;
        ourInfinity->field_196 = 0;
        ourInfinity->nCurrentRainLevel = rainLevel;
        if (bOldRect) {
            ourInfinity->field_68 = oldX1;
            ourInfinity->field_6C = oldY1;
            ourInfinity->field_70 = oldX2;
            ourInfinity->field_74 = oldY2;
        } else {
            ourInfinity->field_68 = -1;
            ourInfinity->field_6C = -1;
            ourInfinity->field_70 = -1;
            ourInfinity->field_74 = -1;
        }

        ourInfinity->ScrollingRequestRect(x1, y1, x2, y2, nScrollState);
        g_pChitin = nullptr;
        ctx.CountInputs(1);

        CompareArrays(ctx, reinterpret_cast<const uint32_t*>(wedData),
            reinterpret_cast<const uint32_t*>(oursImage.data()), kWEDSize / 4);

        const uint8_t originalFlags = Get<uint8_t>(infinity, kInfinityField15E);
        if (originalFlags != ourInfinity->field_15E) {
            ctx.Fail(ctx.Inputs() + ", flags", Hex16(originalFlags), Hex16(ourInfinity->field_15E));
        }

        const int32_t originalField[4] = {
            Get<int32_t>(infinity, kInfinityField68),
            Get<int32_t>(infinity, kInfinityField6C),
            Get<int32_t>(infinity, kInfinityField70),
            Get<int32_t>(infinity, kInfinityField74),
        };
        const int32_t ourField[4] = { ourInfinity->field_68, ourInfinity->field_6C, ourInfinity->field_70, ourInfinity->field_74 };
        if (std::memcmp(originalField, ourField, sizeof(originalField)) != 0) {
            ctx.Fail(ctx.Inputs() + ", rect",
                std::to_string(originalField[0]) + "," + std::to_string(originalField[1]) + "-"
                    + std::to_string(originalField[2]) + "," + std::to_string(originalField[3]),
                std::to_string(ourField[0]) + "," + std::to_string(ourField[1]) + "-"
                    + std::to_string(ourField[2]) + "," + std::to_string(ourField[3]));
        }

        if (Get<int32_t>(infinity, kInfinityField196) != ourInfinity->field_196) {
            ctx.Fail(ctx.Inputs() + ", state", std::to_string(Get<int32_t>(infinity, kInfinityField196)),
                std::to_string(ourInfinity->field_196));
        }

        for (int i = 0; i < kScrollTiles; i++) {
            const uint8_t* pTile = tiles + i * kResInfTileSize;
            const CResInfTile* pOurs = oursTilePointers[i];
            const int32_t originalCount = Get<int32_t>(pTile, kResField44);
            const uint32_t originalDwordFlags = Get<uint32_t>(pTile, kResFlags);
            if (originalCount != pOurs->field_44 || originalDwordFlags != pOurs->dwFlags) {
                char detail[96];
                std::snprintf(detail, sizeof(detail), "tile %d requests %d flags %08X", i, originalCount, originalDwordFlags);
                char oursDetail[96];
                std::snprintf(oursDetail, sizeof(oursDetail), "tile %d requests %d flags %08X", i, pOurs->field_44, pOurs->dwFlags);
                ctx.Fail(ctx.Inputs() + ", " + detail, detail, oursDetail);
            }
        }
    }
}

ORACLE_CASE("infinity/check_tile_state", CheckTileState);
ORACLE_CASE("infinity/scrolling_request_rect", ScrollingRequestRect);
