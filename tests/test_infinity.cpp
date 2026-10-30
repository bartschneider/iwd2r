// The area layer's WED/CInfinity fixtures on a synthetic WED, and
// CTiledObject::CheckTileState (0x54E5B0).
//
// A real WED is built from the packed structs in src/BalDataTypes.h and parsed
// with CResWED::Parse, so the accessors read it exactly as the game's do. The
// CInfinity stands in with one CInfTileSet whose VRAM pool is a plain
// TileDefinition array: CheckTileState's swap path only reads the pool's
// entries and the tiles' m_nVRamTile/m_renderCode, so the fixture needs no
// surfaces. T86/T87 (the scrolling functions) can reuse the same fixture: they
// only add RequestTile/CancelRequestTile, which read the same WED, tile set and
// CResInfTile blocks.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CInfinity.h"
#include "CResWED.h"
#include "CTiledObject.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr DWORD kWEDSize = 0x400;
constexpr DWORD kLayerHeaderOffset = 0x40;
constexpr DWORD kPolyHeaderOffset = 0x60;
constexpr DWORD kTiledObjectsOffset = 0x80;
constexpr DWORD kObjectTileListOffset = 0xC0;
constexpr DWORD kTileDataOffset = 0x100;
constexpr DWORD kTileListOffset = 0x380;
constexpr DWORD kPolyListOffset = 0x390;
constexpr DWORD kPolyPointsOffset = 0x3D8;
constexpr int kTilesAcross = 8;
constexpr int kTilesDown = 8;
constexpr int kTileSetSize = 8;
constexpr int kPolyCount = 4;

// A zeroed block of a class whose constructor/destructor the fixture does not
// run (the game's objects are plain data here; the bits they read are set by
// hand).
template <class T>
struct RawBlock {
    T* get() { return reinterpret_cast<T*>(storage.data()); }

    std::vector<unsigned char> storage = std::vector<unsigned char>(sizeof(T), 0);
};

void Put16(std::vector<BYTE>& image, DWORD at, WORD value)
{
    std::memcpy(image.data() + at, &value, sizeof(value));
}

void Put32(std::vector<BYTE>& image, DWORD at, DWORD value)
{
    std::memcpy(image.data() + at, &value, sizeof(value));
}

// A synthetic WED with one layer, one tiled object and a four-poly poly list,
// plus the CInfinity/tile set/pool the tiled-object functions use. The tile
// data grid is 8x8; each entry's starting tile is i % kTileSetSize and its
// secondary tile (i + 1) % kTileSetSize, so every swap is distinguishable.
struct WedFixture {
    WedFixture()
        : image(kWEDSize, 0)
    {
        std::memcpy(image.data() + 0, "WED ", 4);
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

        for (int i = 0; i < kTilesAcross * kTilesDown; i++) {
            auto* pTileData = reinterpret_cast<WED_TILEDATA*>(image.data() + kTileDataOffset + i * sizeof(WED_TILEDATA));
            // Disjoint primary/secondary pairs (0/1, 2/3, 4/5, 6/7), so one
            // entry's swap cannot change another entry's source render code.
            pTileData->nStartingTile = static_cast<WORD>(2 * (i % 4));
            pTileData->nNumTiles = 1;
            pTileData->nSecondary = static_cast<WORD>(2 * (i % 4) + 1);
            pTileData->bFlags = 0;
            pTileData->wFlags = 0;
        }

        for (int i = 0; i < kTileSetSize; i++) {
            Put16(image, kTileListOffset + 2 * i, static_cast<WORD>(i));
        }

        for (int i = 0; i < kPolyCount; i++) {
            auto* pPoly = reinterpret_cast<WED_POLYLIST*>(image.data() + kPolyListOffset + i * sizeof(WED_POLYLIST));
            pPoly->nType = 0x1;
        }

        SetObject(0, 3, 2, 2);

        m_pResWed->Parse(image.data());
        REQUIRE(m_pResWed->m_bParsed);

        m_pInfinity->pResWED = m_pResWed;

        m_pPool->m_nVTiles = 1;
        m_pPool->m_pTileDefs = m_defs;
        m_defs[0].nTile = 0;
        m_defs[0].nRefCount = 2; // above zero, so a detach never empties it
        m_defs[0].pTileSet = m_pTileSet;

        for (int i = 0; i < kTileSetSize; i++) {
            auto* pTile = m_pResTiles[i];
            pTile->m_nVRamTile = 0;
            pTile->m_renderCode.tileNW = static_cast<BYTE>(0x10 + i);
            pTile->m_renderCode.tileNE = static_cast<BYTE>(0x20 + i);
            pTile->m_renderCode.tileSW = static_cast<BYTE>(0x30 + i);
            pTile->m_renderCode.tileSE = static_cast<BYTE>(0x40 + i);
            pTile->m_pDualTileRes = NULL;
            m_tilePointers[i] = pTile;
        }

        m_pTileSet->m_pVRamPool = m_pPool;
        m_pTileSet->m_pResTiles = m_tilePointers;
        m_pTileSet->m_nTiles = kTileSetSize;

        m_pInfinity->pTileSets[0] = m_pTileSet;

        m_pObject->m_pResWed = m_pResWed;
        m_pObject->m_nWedIndex = 0;
    }

    // The tiled object: which of the object tile list's entries it covers, how
    // many polys of each kind and where their type bytes are (absolute WED
    // offsets, as the game stores them).
    void SetObject(WORD nStartingTile, WORD nNumTiles, WORD nNumPrimaryPolys, WORD nNumSecondaryPolys)
    {
        auto* pObject = reinterpret_cast<WED_TILEDOBJECT*>(image.data() + kTiledObjectsOffset);
        pObject->nStartingTile = nStartingTile;
        pObject->nNumTiles = nNumTiles;
        pObject->nNumPrimaryPolys = nNumPrimaryPolys;
        pObject->nNumSecondaryPolys = nNumSecondaryPolys;
        pObject->nOffsetToPrimaryPolys = kPolyListOffset;
        pObject->nOffsetToSecondaryPolys = kPolyListOffset + 2 * sizeof(WED_POLYLIST);

        for (WORD i = 0; i < nStartingTile + nNumTiles; i++) {
            Put16(image, kObjectTileListOffset + 2 * i, static_cast<WORD>(i));
        }
    }

    BYTE PolyType(int index) const
    {
        return (image.data() + kPolyListOffset + index * sizeof(WED_POLYLIST))[8];
    }

    WED_TILEDATA* TileData(int index)
    {
        return reinterpret_cast<WED_TILEDATA*>(image.data() + kTileDataOffset + index * sizeof(WED_TILEDATA));
    }

    std::vector<BYTE> image;

    RawBlock<CResWED> resWed;
    RawBlock<CInfinity> infinity;
    RawBlock<CInfTileSet> tileSet;
    RawBlock<CVRamPool> pool;
    RawBlock<CResInfTile> resTiles[kTileSetSize];
    RawBlock<CTiledObject> object;

    CResWED* m_pResWed = resWed.get();
    CInfinity* m_pInfinity = infinity.get();
    CInfTileSet* m_pTileSet = tileSet.get();
    CVRamPool* m_pPool = pool.get();
    CResInfTile* m_pResTiles[kTileSetSize] = {
        resTiles[0].get(),
        resTiles[1].get(),
        resTiles[2].get(),
        resTiles[3].get(),
        resTiles[4].get(),
        resTiles[5].get(),
        resTiles[6].get(),
        resTiles[7].get(),
    };
    CTiledObject* m_pObject = object.get();
    CResInfTile** m_tilePointers = m_pResTiles;
    TileDefinition m_defs[1] = {};
};

}

// CTiledObject::CheckTileState (0x54E5B0): when the AI state differs from the
// rendered state the object's tiles take the new state's wFlags and swap their
// VRAM images (primary <-> secondary), and the tiled object's primary polys
// clear the type bit while its secondary polys set it (the reverse for the
// secondary state).
TEST_CASE("infinity: CheckTileState swaps the tile images and the poly types", "[infinity]")
{
    SECTION("primary state")
    {
        WedFixture fixture;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;

        const BYTE primaryBefore = fixture.PolyType(0);
        const BYTE secondaryBefore = fixture.PolyType(2);

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.m_pObject->m_wRenderState == CTiledObject::STATE_PRIMARY_TILE);
        for (int i = 0; i < 3; i++) {
            const WED_TILEDATA* pTileData = fixture.TileData(i);
            CHECK(pTileData->wFlags == CTiledObject::STATE_PRIMARY_TILE);

            // The swap detached nSecondary and attached tileList[nStartingTile],
            // so the primary tile carries the secondary's code.
            CHECK(fixture.m_pResTiles[pTileData->nStartingTile]->m_renderCode.tileNW
                == static_cast<BYTE>(0x10 + pTileData->nSecondary));
        }

        CHECK((fixture.PolyType(0) & 0x2) == 0); // primary poly: cleared
        CHECK((fixture.PolyType(1) & 0x2) == 0);
        CHECK((fixture.PolyType(2) & 0x2) != 0); // secondary poly: set
        CHECK((fixture.PolyType(3) & 0x2) != 0);
        CHECK((primaryBefore & 0x2) == 0);
        CHECK((secondaryBefore & 0x2) == 0);
    }

    SECTION("secondary state")
    {
        WedFixture fixture;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_SECONDARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_PRIMARY_TILE;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.m_pObject->m_wRenderState == CTiledObject::STATE_SECONDARY_TILE);
        for (int i = 0; i < 3; i++) {
            const WED_TILEDATA* pTileData = fixture.TileData(i);
            CHECK(pTileData->wFlags == CTiledObject::STATE_SECONDARY_TILE);

            // The reverse swap: the secondary image gets the primary's code.
            CHECK(fixture.m_pResTiles[pTileData->nSecondary]->m_renderCode.tileNW
                == static_cast<BYTE>(0x10 + pTileData->nStartingTile));
        }

        CHECK((fixture.PolyType(0) & 0x2) != 0); // primary poly: set
        CHECK((fixture.PolyType(1) & 0x2) != 0);
        CHECK((fixture.PolyType(2) & 0x2) == 0); // secondary poly: cleared
        CHECK((fixture.PolyType(3) & 0x2) == 0);
    }
}

TEST_CASE("infinity: CheckTileState's early outs and edge counts", "[infinity]")
{
    SECTION("same state draws nothing")
    {
        WedFixture fixture;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_PRIMARY_TILE;
        const std::vector<BYTE> before = fixture.image;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.image == before);
        CHECK(fixture.m_pResTiles[1]->m_renderCode.tileNW == 0x11);
    }

    SECTION("no WED index draws nothing")
    {
        WedFixture fixture;
        fixture.m_pObject->m_nWedIndex = -1;
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;
        const std::vector<BYTE> before = fixture.image;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.image == before);
        CHECK(fixture.m_pObject->m_wRenderState == CTiledObject::STATE_SECONDARY_TILE);
    }

    SECTION("zero and -1 poly counts")
    {
        WedFixture fixture;
        fixture.SetObject(0, 3, 0, 0xFFFF);
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK((fixture.PolyType(0) & 0x2) == 0); // untouched (count 0)
        CHECK((fixture.PolyType(2) & 0x2) == 0); // untouched (0xFFFF)
    }

    SECTION("no tiles only toggles the polys")
    {
        WedFixture fixture;
        fixture.SetObject(0, 0, 2, 2);
        fixture.m_pObject->m_wAIState = CTiledObject::STATE_PRIMARY_TILE;
        fixture.m_pObject->m_wRenderState = CTiledObject::STATE_SECONDARY_TILE;

        fixture.m_pObject->CheckTileState(*fixture.m_pInfinity);

        CHECK(fixture.TileData(0)->wFlags == 0);
        CHECK((fixture.PolyType(2) & 0x2) != 0);
    }
}
