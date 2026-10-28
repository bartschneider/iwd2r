#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CGameAnimationType.h"
#include "CGameAnimationTypeAmbient.h"
#include "CGameAnimationTypeAmbientStatic.h"
#include "CGameAnimationTypeCharacter.h"
#include "CGameAnimationTypeFlying.h"
#include "CGameAnimationTypeMonster.h"
#include "CGameAnimationTypeMonsterAnkheg.h"
#include "CGameAnimationTypeMonsterIcewind.h"
#include "CGameAnimationTypeMonsterIcewindNew.h"
#include "CGameAnimationTypeMonsterLarge.h"
#include "CGameAnimationTypeMonsterLarge16.h"
#include "CGameAnimationTypeMonsterMulti.h"
#include "CGameAnimationTypeMonsterOld.h"
#include "CGameAnimationTypeMonsterQuadrant.h"
#include "CGameAnimationTypeTownStatic.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameObjectArray.h"
#include "CGameTrigger.h"
#include "CVidInf.h"
#include "CVidPoly.h"
#include "CWeather.h"
#include "engine_world.h"
#include "render_surfaces.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

// The Render functions of the creature animations that had none
// (CGameAnimationTypeMonster 0x6AC310, MonsterOld 0x6B03E0, MonsterMulti
// 0x6B71F0) or a stub (MonsterAnkheg 0x6C3910), drawn with the game's BAMs
// through a standalone CInfinity into DirectDraw shim surfaces: the back
// buffer and the two FX surfaces, keyed on pure green as CVidInf's
// CreateSurfaces keys them. The pixels that change must lie in the bounds
// the animation reports for the frame, a creature that is not fading must be
// drawn solid (FXUnlock gets NULL: a rectangle would dither half of it
// away), and the tint must keep its red. Then the same creatures from the
// game's CRE files standing in AR1000 of a new game, drawn by the area's
// whole render (tiles through VRAM surfaces the test makes, then each
// object), and the door and container highlights drawn there
// (CGameDoor::Render 0x488B30, CGameContainer::Render 0x47F580). With
// IWD2R_SHOT_DIR set, each render is also written there as a PNG. Every
// engine works on a private view of the install (tests/data_view.h).

using namespace engine_world;
using namespace render_surfaces;

namespace {

// Renders the animation's current frame at `pos` as CGameSprite::Render
// does (its FX rectangle and reference point, then its bounds) and returns
// what changed. rBounds receives the bounds the animation reports.
Drawn Render(RenderScope& scope, CGameAnimationType* pAnimation, const CPoint& pos, BOOL bFadeOut, COLORREF rgbTint, CRect& rBounds, const std::string& shot)
{
    scope.Back(true, 0);
    CRect rFx;
    CPoint ptReference;
    pAnimation->CalculateFxRect(rFx, ptReference, 0);
    pAnimation->CalculateGCBoundsRect(rBounds, pos, ptReference, 0, rFx.Width(), rFx.Height());
    pAnimation->Render(scope.Infinity(),
        scope.VidMode(),
        CVIDINF_SURFACE_BACK,
        rFx,
        pos,
        ptReference,
        0,
        rgbTint,
        rBounds,
        FALSE,
        bFadeOut,
        0,
        0);
    const std::vector<DWORD> pixels = scope.Back();
    Shot(pixels, shot);
    return Count(pixels, rBounds);
}

// The checks every class shares: solid within its bounds, dithered when
// fading, and a tint without blue keeps the red.
void CheckRender(RenderScope& scope, CGameAnimationType* pAnimation, const CPoint& pos, const std::string& name)
{
    CRect rBounds;
    const Drawn solid = Render(scope, pAnimation, pos, FALSE, RGB(255, 255, 255), rBounds, name);
    INFO("bounds " << rBounds.left << "," << rBounds.top << " - " << rBounds.right << "," << rBounds.bottom);
    INFO("drawn " << solid.changed << ", outside " << solid.outside);
    CHECK(rBounds.Width() > 0);
    CHECK(rBounds.Height() > 0);
    CHECK(solid.changed > 100);
    CHECK(solid.outside == 0);

    // Fading: FXUnlock fills the FX rectangle with the transparent colour
    // through a dithered FillPoly, so about half the pixels stay black.
    const Drawn faded = Render(scope, pAnimation, pos, TRUE, RGB(255, 255, 255), rBounds, name + "_faded");
    INFO("faded " << faded.changed);
    CHECK(faded.changed > 0);
    CHECK(faded.outside == 0);
    CHECK(faded.changed * 4 < solid.changed * 3);
    CHECK(faded.changed * 4 > solid.changed);

    // A yellow tint (no blue): each channel is the cell's plus the tint's
    // less 255, so blue goes to 0 and red stays (red from the low byte).
    const Drawn yellow = Render(scope, pAnimation, pos, FALSE, RGB(255, 255, 0), rBounds, name + "_yellow");
    INFO("yellow " << yellow.changed << ", with red " << yellow.red << ", with blue " << yellow.blue);
    CHECK(yellow.changed > 0);
    CHECK(yellow.outside == 0);
    CHECK(yellow.blue == 0);
    CHECK(yellow.red > yellow.changed / 2);
}

// The quadrants of a multi-cell animation each drawn from their own cell:
// each cell is tinted its own primary (red, green, blue in turn), so every
// pixel that lies in one quadrant's bounds only carries that quadrant's
// channel alone. The cells' tints are put back afterwards.
void CheckQuadrants(RenderScope& scope, CGameAnimationType* pAnimation, CVidCell* pCells, int nQuadrants, const CPoint& pos, const std::string& name)
{
    static const COLORREF tints[3] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255) };
    static const DWORD channels[3] = { 0xFF0000, 0x00FF00, 0x0000FF };
    std::vector<COLORREF> saved;
    std::vector<CRect> bounds;
    for (int quad = 0; quad < nQuadrants; quad++) {
        saved.push_back(pCells[quad].GetTintColor());
        pCells[quad].SetTintColor(tints[quad % 3]);
        CPoint ptCenter;
        CSize frameSize;
        REQUIRE(pCells[quad].GetCurrentCenterPoint(ptCenter, FALSE));
        REQUIRE(pCells[quad].GetCurrentFrameSize(frameSize, FALSE));
        CRect rQuad;
        pAnimation->CalculateGCBoundsRect(rQuad, pos, ptCenter, 0, frameSize.cx, frameSize.cy);
        bounds.push_back(rQuad);
    }

    CRect rBounds;
    Render(scope, pAnimation, pos, FALSE, RGB(255, 255, 255), rBounds, name);
    const std::vector<DWORD> pixels = scope.Back();
    std::vector<int> own(nQuadrants, 0);
    std::vector<int> wrong(nQuadrants, 0);
    for (int y = 0; y < kHeight; y++) {
        for (int x = 0; x < kWidth; x++) {
            const DWORD pixel = pixels[y * kWidth + x] & 0xFFFFFF;
            if (pixel == 0) {
                continue;
            }
            int found = -1;
            int count = 0;
            for (int quad = 0; quad < nQuadrants; quad++) {
                if (bounds[quad].PtInRect(CPoint(x, y))) {
                    found = quad;
                    count++;
                }
            }
            if (count == 1) {
                if ((pixel & ~channels[found % 3]) != 0) {
                    wrong[found]++;
                } else {
                    own[found]++;
                }
            }
        }
    }
    // A quadrant can lie under the others' bounds or be empty in a frame
    // (the dragon's sixth facing south): most must show their own colour.
    int shown = 0;
    for (int quad = 0; quad < nQuadrants; quad++) {
        INFO(name << ", quadrant " << quad << ": bounds " << bounds[quad].left << "," << bounds[quad].top << " - " << bounds[quad].right << ","
                  << bounds[quad].bottom << ", its own colour " << own[quad] << ", another " << wrong[quad]);
        CHECK(wrong[quad] == 0);
        if (own[quad] > 0) {
            shown++;
        }
        pCells[quad].SetTintColor(saved[quad]);
    }
    INFO(name << ": " << shown << " of " << nQuadrants << " quadrants show their own colour");
    CHECK(shown * 2 >= nQuadrants);
}

bool Begin(std::string& why)
{
    if (DataDir() == nullptr) {
        why = "IWD2_DATA unset";
        return false;
    }
    std::string error;
    if (!StartEngine(error)) {
        why = error;
        return false;
    }
    return true;
}

}

// A mind flayer (0x7F01, MMIN's split BAMs): MonsterOld::NewSupport accepts
// it, so the factory makes a CGameAnimationTypeMonster. Facing south (0) and
// north-east (12, mirrored: MIRROR_FX).
TEST_CASE("creature render: CGameAnimationTypeMonster draws a mind flayer", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing : { 0, 12 }) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x7F01, colors, facing));
        REQUIRE(dynamic_cast<CGameAnimationTypeMonster*>(pAnimation.get()) != nullptr);
        CheckRender(scope, pAnimation.get(), CPoint(320, 300), "mind_flayer_" + std::to_string(facing));
    }
}

// A wolf (0x7B00, MWLF): NewSupport refuses it, so the factory makes a
// CGameAnimationTypeMonsterOld.
TEST_CASE("creature render: CGameAnimationTypeMonsterOld draws a wolf", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing : { 0, 12 }) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x7B00, colors, facing));
        REQUIRE(dynamic_cast<CGameAnimationTypeMonsterOld*>(pAnimation.get()) != nullptr);
        CheckRender(scope, pAnimation.get(), CPoint(320, 300), "wolf_" + std::to_string(facing));
    }
}

// An ankheg (0x3000). The game has no MAKH BAMs (no creature uses the
// class), so its body and hole cells are given a wolf's and a spider's
// BAMs. Each cell is drawn around its own frame centre: the bounds are the
// union of the two cells' bounds.
TEST_CASE("creature render: CGameAnimationTypeMonsterAnkheg draws its hole and its body", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x3000, colors, 0));
    auto* pAnkheg = dynamic_cast<CGameAnimationTypeMonsterAnkheg*>(pAnimation.get());
    REQUIRE(pAnkheg != nullptr);
    CHECK_FALSE(pAnkheg->IsFalseColor());
    pAnkheg->m_g1VidCellBase.SetResRef(CResRef("MWLFG1"), FALSE, TRUE, TRUE);
    pAnkheg->m_g1VidCellHoleBase.SetResRef(CResRef("MSPIG1"), FALSE, TRUE, TRUE);
    pAnkheg->m_currentBamSequence = 0;
    pAnkheg->ChangeDirection(0);

    const CPoint pos(320, 300);
    CRect rBounds;
    for (CVidCell* pCell : { pAnkheg->m_currentVidCellHole, pAnkheg->m_currentVidCell }) {
        CPoint ptCenter;
        CSize frameSize;
        REQUIRE(pCell->GetCurrentCenterPoint(ptCenter, FALSE));
        REQUIRE(pCell->GetCurrentFrameSize(frameSize, FALSE));
        CRect rCell;
        pAnkheg->CalculateGCBoundsRect(rCell, pos, ptCenter, 0, frameSize.cx, frameSize.cy);
        rBounds.UnionRect(rBounds, rCell);
    }
    INFO("bounds " << rBounds.left << "," << rBounds.top << " - " << rBounds.right << "," << rBounds.bottom);

    auto draw = [&](BOOL bFadeOut, COLORREF rgbTint, const std::string& name) {
        scope.Back(true, 0);
        pAnkheg->Render(scope.Infinity(), scope.VidMode(), CVIDINF_SURFACE_BACK, CRect(), pos, CPoint(), 0, rgbTint, CRect(), FALSE, bFadeOut, 0, 0);
        const std::vector<DWORD> pixels = scope.Back();
        Shot(pixels, name);
        return Count(pixels, rBounds);
    };
    const Drawn solid = draw(FALSE, RGB(255, 255, 255), "ankheg");
    INFO("drawn " << solid.changed << ", outside " << solid.outside);
    CHECK(solid.changed > 100);
    CHECK(solid.outside == 0);
    const Drawn faded = draw(TRUE, RGB(255, 255, 255), "ankheg_faded");
    INFO("faded " << faded.changed);
    CHECK(faded.changed > 0);
    CHECK(faded.changed * 4 < solid.changed * 3);
    // The ankheg sets the tint itself on both cells (no blue: none left).
    const Drawn yellow = draw(FALSE, RGB(255, 255, 0), "ankheg_yellow");
    INFO("yellow " << yellow.changed << ", with red " << yellow.red << ", with blue " << yellow.blue);
    CHECK(yellow.blue == 0);
    CHECK(yellow.red > yellow.changed / 2);
    CHECK(pAnkheg->m_currentVidCell->GetTintColor() == RGB(255, 255, 0));
    CHECK(pAnkheg->m_currentVidCellHole->GetTintColor() == RGB(255, 255, 0));
}

// The black dragon (0x1201, 60GUARDI.CRE): a CGameAnimationTypeMonsterMulti
// of nine quadrants with split BAMs, MDR2 + group + quadrant + sequence
// character + direction character. Its ChangeDirection (0x6B5D80) and
// SetSequence (0x6B7660) were missing too: the constructor's facing never
// reached the cells, and no sequence changed them. The groups each
// sequence picks are the ones the game data has (MDR21xx0 .. MDR25xx1).
TEST_CASE("creature render: CGameAnimationTypeMonsterMulti draws the black dragon's quadrants", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x1201, colors, 0));
    auto* pDragon = dynamic_cast<CGameAnimationTypeMonsterMulti*>(pAnimation.get());
    REQUIRE(pDragon != nullptr);
    REQUIRE(pDragon->m_nQuadrants == 9);
    CHECK(pDragon->m_currentBamDirection == 0);
    CHECK(pDragon->m_currentVidCell == pDragon->m_g2VidCellBase);
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR22100");
    CHECK(pDragon->m_currentVidCell[8].GetResRef().GetResRefStr() == "MDR22900");
    CheckRender(scope, pDragon, CPoint(320, 330), "dragon_0");

    // Sequence 9: group 4 with the sequence character 2.
    CHECK(pDragon->SetSequence(9) == 9);
    CHECK(pDragon->m_currentVidCell == pDragon->m_g4VidCellBase);
    CHECK(pDragon->m_currentBamSequence == 2);
    CHECK(pDragon->field_44A == &pDragon->field_67C);
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR24120");
    CHECK(pDragon->field_800 == 9);
    // Sequences 14 and 15 become 7 (group 2), 11 to 13 become 0 (group 3).
    CHECK(pDragon->SetSequence(14) == 7);
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR22100");
    CHECK(pDragon->SetSequence(11) == 0);
    CHECK(pDragon->m_currentVidCell == pDragon->m_g3VidCellBase);
    CHECK(pDragon->m_currentVidCell[4].GetResRef().GetResRefStr() == "MDR23500");

    // Facing 12 (north-east) mirrors direction 4: the files of direction 4,
    // sequence 9 * 0 - 12 + 16 = 4.
    pDragon->ChangeDirection(12);
    CHECK(pDragon->m_splitDirectionBamChar == '4');
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR23104");
    CHECK(pDragon->m_currentVidCell[0].m_nCurrentSequence == 4);
    CHECK(pDragon->IsMirroring());
    CheckRender(scope, pDragon, CPoint(320, 330), "dragon_12");

    // Each of the nine quadrants is its own cell, at its own place.
    CheckQuadrants(scope, pDragon, pDragon->m_currentVidCell, pDragon->m_nQuadrants, CPoint(320, 330), "dragon_12_quadrants");
    pDragon->ChangeDirection(0);
    CheckQuadrants(scope, pDragon, pDragon->m_currentVidCell, pDragon->m_nQuadrants, CPoint(320, 330), "dragon_0_quadrants");
}

// The death sound by animation family (0x6E8180). The constructor's switch on
// `animationID & 0xF00` writes m_pSndDeath at six sites; the exe's are all the
// same instruction, `mov dword ptr [esi+0x1e], <literal>`, and there are two
// literals: 0x8B63D0 "FAL_01B" at 0x6E86FE (family 0x200), 0x6E8967 (0x300)
// and 0x6E8A29 (0x700); 0x8B6400 "FAL_03B" at 0x6E87FB (0x000), 0x6E8BB2
// (0x500) and 0x6E8C84 (0x400). Family 0x100 sets none and keeps the base
// class's. The factory picks this class for `animationID & 0xF000 == 0xF000`;
// family 0x700 accepts only the low nibbles 0x70, 0x80 and 0x90 (its default
// asserts).
TEST_CASE("creature render: MonsterIcewindNew keeps the exe's death sound by family", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    BYTE colors[16] = {};
    struct Family {
        USHORT nAnimationID; // 0xF000 | family | low nibble
        const char* pSndDeath;
    };
    const Family families[] = {
        { 0xF000, "FAL_03B" },
        { 0xF200, "FAL_01B" },
        { 0xF300, "FAL_01B" },
        { 0xF400, "FAL_03B" },
        { 0xF500, "FAL_03B" },
        { 0xF770, "FAL_01B" },
    };
    for (const Family& family : families) {
        INFO("animation id 0x" << std::hex << family.nAnimationID);
        std::unique_ptr<CGameAnimationType> pAnimation(
            CGameAnimationType::SetAnimationType(family.nAnimationID, colors, 0));
        auto* pMonster = dynamic_cast<CGameAnimationTypeMonsterIcewindNew*>(pAnimation.get());
        REQUIRE(pMonster != nullptr);
        CHECK(std::string(pMonster->GetSndDeath()) == family.pSndDeath);
    }
}

// The white wyvern (0x1001, MWYV in four quadrant BAMs, 50WYRWH and six
// other CRE files): CGameAnimationTypeMonsterQuadrant::Render (0x6BC5F0)
// draws each quadrant from its own cell, m_currentVidCell + quad * 0xDA
// (0x6BC84E, 0x6BC856), with that cell's tint read and restored (0x6BC799,
// 0x6BC86A). Ours drew cell 0 in every quadrant with cell 0's tint, so the
// per-quadrant colours of CheckQuadrants came out all red.
TEST_CASE("creature render: CGameAnimationTypeMonsterQuadrant draws each quadrant from its own cell", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing : { 0, 12 }) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x1001, colors, facing));
        auto* pWyvern = dynamic_cast<CGameAnimationTypeMonsterQuadrant*>(pAnimation.get());
        REQUIRE(pWyvern != nullptr);
        REQUIRE(pWyvern->m_nQuadrants == 4);
        CheckQuadrants(scope, pWyvern, pWyvern->m_currentVidCell, pWyvern->m_nQuadrants, CPoint(320, 300),
            "wyvern_" + std::to_string(facing) + "_quadrants");
    }
}

// The tint of the eight Renders ported before this stream's (Ambient
// 0x6A8470, AmbientStatic 0x6A6A30, Flying 0x6A44F0, MonsterIcewind 0x6E7A40,
// MonsterLarge 0x6B1E70, MonsterLarge16 0x6B3C60, MonsterQuadrant 0x6BC5F0,
// TownStatic 0x6A55C0): each channel is the cell's and the tint's added less
// 255, red from the low bytes (for instance 0x6A85BC). Ours took red from the
// blue bytes, so a yellow tint (blue 0) left no red at all.
TEST_CASE("creature render: the earlier Renders keep red in a yellow tint", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    struct Case {
        USHORT animationID;
        const char* name;
        bool (*isClass)(CGameAnimationType*);
    };
    const Case cases[] = {
        { 0xC100, "Ambient (cat)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeAmbient*>(p) != nullptr; } },
        { 0xB000, "AmbientStatic (cow)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeAmbientStatic*>(p) != nullptr; } },
        { 0xD400, "Flying (bird, the one Flying animation that is tinted)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeFlying*>(p) != nullptr; } },
        { 0xE408, "MonsterIcewind (goblin)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeMonsterIcewind*>(p) != nullptr; } },
        { 0x9000, "MonsterLarge (ogre)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeMonsterLarge*>(p) != nullptr; } },
        { 0xA000, "MonsterLarge16 (wyvern)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeMonsterLarge16*>(p) != nullptr; } },
        { 0x1001, "MonsterQuadrant (white wyvern)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeMonsterQuadrant*>(p) != nullptr; } },
        { 0x4400, "TownStatic (townsman)", [](CGameAnimationType* p) { return dynamic_cast<CGameAnimationTypeTownStatic*>(p) != nullptr; } },
    };
    for (const Case& c : cases) {
        INFO(c.name);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(c.animationID, colors, 0));
        REQUIRE(c.isClass(pAnimation.get()));
        CRect rBounds;
        const Drawn white = Render(scope, pAnimation.get(), CPoint(320, 300), FALSE, RGB(255, 255, 255), rBounds, "");
        INFO("white: drawn " << white.changed << ", with red " << white.red);
        REQUIRE(white.changed > 50);
        CHECK(white.red > white.changed / 2);
        const Drawn yellow = Render(scope, pAnimation.get(), CPoint(320, 300), FALSE, RGB(255, 255, 0), rBounds, "");
        INFO("yellow: drawn " << yellow.changed << ", with red " << yellow.red << ", with blue " << yellow.blue);
        CHECK(yellow.changed > 50);
        CHECK(yellow.blue == 0);
        CHECK(yellow.red > yellow.changed / 2);
    }
}

namespace {

// A new game in AR1000 with its party member, the area explored and
// visible, drawn into AreaScope's surfaces.
struct World {
    CGameSprite* pCharacter = NULL;
    CGameArea* pArea = NULL;
};

bool NewWorld(World& world, std::string& why)
{
    if (!Begin(why)) {
        return false;
    }
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        why = "no CHR V2.2 file in the Characters folder";
        return false;
    }
    world.pCharacter = NewGameInStartArea(cre);
    world.pArea = world.pCharacter->GetArea();
    if (world.pArea == NULL) {
        why = "the party member is in no area";
        return false;
    }
    world.pArea->m_visibility.SetAreaExplored();
    world.pArea->m_visibility.SetAreaVisible(TRUE);
    return true;
}

// The first object of `type` on one of the area's lists for which `accept`
// holds.
template <class T, class F>
T* FindObject(CTypedPtrList<CPtrList, int*>& list, BYTE type, F accept)
{
    CGameObjectArray* pObjects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    POSITION pos = list.GetHeadPosition();
    while (pos != NULL) {
        LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list.GetNext(pos)));
        CGameObject* pObject;
        if (pObjects->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
            pObjects->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            if (pObject->GetObjectType() == type && accept(static_cast<T*>(pObject))) {
                return static_cast<T*>(pObject);
            }
        }
    }
    return NULL;
}

}

// Creatures of the three reachable classes, from the game's own files,
// standing in AR1000 next to a new game's party member and drawn by the
// area's render (CGameArea::Render, then CGameSprite::Render for each)
// with the WED's wall polygons, the area's tint and the viewport: a mind
// flayer (53ZEPH, 0x7F01: CGameAnimationTypeMonster), a wolf (12WOLF,
// 0x7B00: MonsterOld) and the black dragon (60GUARDI, 0x1201:
// MonsterMulti). The area is explored and visible and each creature seen
// for a while, so none is drawn faded. Each creature changes the area's
// picture inside its bounds: the render with it against the render with
// it hidden (CreatureHidden, base stats 0x294, which the wolf and the
// dragon have set in their files and which is cleared here).
TEST_CASE("creature render: creatures of the new Renders stand visible in AR1000", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    const CPoint pos = world.pCharacter->GetPos();

    struct Creature {
        const char* resRef;
        USHORT animationID;
        CPoint offset;
        CGameSprite* pSprite;
    };
    Creature creatures[] = {
        { "53ZEPH", 0x7F01, CPoint(-160, 40), NULL },
        { "12WOLF", 0x7B00, CPoint(110, 50), NULL },
        { "60GUARDI", 0x1201, CPoint(260, 120), NULL },
    };
    for (Creature& c : creatures) {
        c.pSprite = AddCreature(pArea, c.resRef, CPoint(pos.x + c.offset.x, pos.y + c.offset.y));
        REQUIRE(c.pSprite != NULL);
        CHECK(c.pSprite->GetAnimation()->GetAnimationId() == c.animationID);
        c.pSprite->m_canBeSeen = CGameObject::VISIBLE_DELAY + 10;
        c.pSprite->m_baseStats.field_294 = 0;
    }
    pArea->SortLists();

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(CPoint(pos.x + 60, pos.y + 20));
    const std::vector<DWORD> all = scope.RenderArea(pArea);
    scope.Shot(all, "ar1000_world");

    for (Creature& c : creatures) {
        INFO(c.resRef);
        c.pSprite->m_baseStats.field_294 = 1;
        const std::vector<DWORD> without = scope.RenderArea(pArea);
        c.pSprite->m_baseStats.field_294 = 0;

        CRect rFx;
        CPoint ptReference;
        c.pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, c.pSprite->m_posZ);
        // The sprite is drawn at its position lowered by the height map
        // (CGameSprite::Render 0x70396F).
        const CPoint ptDrawn(c.pSprite->GetPos().x, c.pSprite->GetPos().y + pArea->GetHeightOffset(c.pSprite->GetPos(), c.pSprite->m_listType));
        CRect rBounds;
        c.pSprite->GetAnimation()->CalculateGCBoundsRect(rBounds, ptDrawn, ptReference, c.pSprite->m_posZ, rFx.Width(), rFx.Height());
        const CRect rScreen = scope.ToScreen(rBounds);
        INFO("bounds on screen " << rScreen.left << "," << rScreen.top << " - " << rScreen.right << "," << rScreen.bottom);
        const Difference inside = Compare(without, all, scope.Screen(), rScreen);
        INFO("changed inside " << inside.changed);
        CHECK(inside.changed > 300);

        // Nothing outside the creature's bounds and the ring the sprite
        // draws around its feet (its ellipse, CGameAnimation::GetEllipseRect)
        // depends on it.
        CRect rEllipse(c.pSprite->GetAnimation()->GetEllipseRect());
        rEllipse.OffsetRect(c.pSprite->GetPos());
        rEllipse.InflateRect(2, 2);
        CRect rAround;
        rAround.UnionRect(rBounds, rEllipse);
        const Difference around = Compare(without, all, scope.Screen(), scope.ToScreen(rAround));
        const Difference whole = Compare(without, all, scope.Screen(), CRect(0, 0, scope.Screen().Width(), scope.Screen().Height()));
        CHECK(whole.changed == around.changed);
    }
}

// The door and container highlights (CGameDoor::Render 0x488B30,
// CGameContainer::Render 0x47F580) on a closed door and a container of
// AR1000. With m_drawPoly set (the outline the game draws for a while, as
// after a script's SetDrawPoly) the polygon is outlined in pure red
// (CInfinity::OutlinePoly) and, with translucent blits on, filled through
// the FX surface (FillClosedPoly 0x48B3C0, FillPoly 0x481A10, then
// CVidInf::FXRender 0x79D680 with flags 0x10) in the red PackColor 0x79ADF0
// packs. Flag 0x10 is DrawHLineShaded32: each pixel of the polygon blended
// towards the colour by its brightness, so on a uniform grey every pixel
// of the polygon becomes the same reddish grey and the dark ones of the
// area stay dark.
TEST_CASE("creature render: AR1000's doors and containers draw their highlight", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    CGameDoor* pDoor = FindObject<CGameDoor>(pArea->m_lVertSort, CGameObject::TYPE_DOOR, [](CGameDoor* p) {
        return (p->m_dwFlags & 0x1) == 0 && (p->m_dwFlags & 0x2080) == 0 && p->m_nClosedPolygon > 2;
    });
    REQUIRE(pDoor != NULL);
    CGameContainer* pContainer = FindObject<CGameContainer>(pArea->m_lVertSortBack, CGameObject::TYPE_CONTAINER, [](CGameContainer* p) {
        return p->m_containerType != 4 && p->m_nPolygon > 2;
    });
    REQUIRE(pContainer != NULL);
    // The rectangles the fills walk come from the constructors through
    // CGameAIBase::sub_481890 (0x481890): an object at most 512 x 512 gets
    // its own bounding rectangle, a larger one new 512 x 512 pieces of it.
    REQUIRE(pDoor->field_6DA.GetSize() == 1);
    CHECK(pDoor->field_6DA[0] == &pDoor->m_rClosedBounding);
    REQUIRE(pContainer->field_8D6.GetSize() == 1);
    CHECK(pContainer->field_8D6[0] == &pContainer->m_rBounding);
    // A larger rectangle (no door or container of AR1000 has one): 3 x 2
    // pieces, row by row, the last column and row cut at the edges.
    {
        CRect r(100, 50, 1300, 700);
        CArray<CRect*> pieces;
        pDoor->sub_481890(&r, pieces);
        REQUIRE(pieces.GetSize() == 6);
        long long area = 0;
        for (int n = 0; n < pieces.GetSize(); n++) {
            const CRect* pPiece = pieces[n];
            INFO("piece " << n << ": " << pPiece->left << "," << pPiece->top << " - " << pPiece->right << "," << pPiece->bottom);
            CHECK(pPiece->left == r.left + 512 * (n % 3));
            CHECK(pPiece->top == r.top + 512 * (n / 3));
            CHECK(pPiece->right == std::min(pPiece->left + 512, r.right));
            CHECK(pPiece->bottom == std::min(pPiece->top + 512, r.bottom));
            area += static_cast<long long>(pPiece->Width()) * pPiece->Height();
            delete pPiece;
        }
        CHECK(area == static_cast<long long>(r.Width()) * r.Height());
    }

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    const BOOLEAN translucentBefore = CInfinity::TRANSLUCENT_BLTS_ON;
    const CRect whole(0, 0, scope.Screen().Width(), scope.Screen().Height());
    // Pure red packed for the 32-bit surface (shifts 16/8/0).
    REQUIRE(scope.VidMode()->PackColor(RGB(255, 0, 0)) == 0xFF0000);

    struct Case {
        const char* name;
        CRect rBounding;
        SHORT* pDrawPoly;
        std::function<void()> fill;
    };
    const Case cases[] = {
        { "door", pDoor->m_rClosedBounding, &pDoor->m_drawPoly, [&] {
             pDoor->FillClosedPoly(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK, scope.VidMode()->PackColor(RGB(255, 0, 0)));
         } },
        { "container", pContainer->m_rBounding, &pContainer->m_drawPoly, [&] {
             pContainer->FillPoly(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK, scope.VidMode()->PackColor(RGB(255, 0, 0)));
         } },
    };
    for (const Case& c : cases) {
        INFO(c.name);
        scope.CentreOn(c.rBounding.CenterPoint());
        const CRect rScreen = scope.ToScreen(c.rBounding);
        CRect rAround(rScreen);
        rAround.InflateRect(1, 1);
        INFO("bounds on screen " << rScreen.left << "," << rScreen.top << " - " << rScreen.right << "," << rScreen.bottom);

        // The fill alone on grey: (255 * 85 + 170 * 0x80) >> 8 = 0xA9 red,
        // 170 * 0x80 >> 8 = 0x55 green and blue.
        const std::vector<DWORD> grey = scope.Back(true, 0x808080);
        c.fill();
        const std::vector<DWORD> greyFilled = scope.Back();
        scope.Shot(greyFilled, std::string("ar1000_") + c.name + "_fill");
        const Difference shaded = Compare(grey, greyFilled, scope.Screen(), rScreen, 0xA95555);
        INFO("fill on grey: changed " << shaded.changed << ", reddish grey " << shaded.exact);
        CHECK(shaded.changed > 50);
        CHECK(shaded.exact == shaded.changed);
        CHECK(Compare(grey, greyFilled, scope.Screen(), whole).changed == shaded.changed);

        // In the area: the outline alone (translucent blits off), then the
        // outline and the fill.
        *c.pDrawPoly = 0;
        CInfinity::TRANSLUCENT_BLTS_ON = TRUE;
        const std::vector<DWORD> plain = scope.RenderArea(pArea);
        *c.pDrawPoly = 100;
        CInfinity::TRANSLUCENT_BLTS_ON = FALSE;
        const std::vector<DWORD> outlined = scope.RenderArea(pArea);
        CInfinity::TRANSLUCENT_BLTS_ON = TRUE;
        const std::vector<DWORD> filled = scope.RenderArea(pArea);
        scope.Shot(filled, std::string("ar1000_") + c.name + "_highlight");
        *c.pDrawPoly = 0;

        const Difference outline = Compare(plain, outlined, scope.Screen(), rAround, 0xFF0000);
        INFO("outline: changed " << outline.changed << ", red " << outline.exact);
        CHECK(outline.exact > 20);
        CHECK(outline.changed <= outline.exact);
        CHECK(Compare(plain, outlined, scope.Screen(), whole).changed == outline.changed);
        // The fill changes nothing outside the polygon's rectangle.
        const Difference fill = Compare(outlined, filled, scope.Screen(), rAround);
        INFO("fill in the area: changed " << fill.changed);
        CHECK(fill.changed > 0);
        CHECK(Compare(outlined, filled, scope.Screen(), whole).changed == fill.changed);
    }
    CInfinity::TRANSLUCENT_BLTS_ON = translucentBefore;
}

namespace {

// The pixel DrawHLineShaded32 (flag 0x10) makes of a grey pixel `grey` and
// the colour 0x00RRGGBB: each channel blended towards the colour by the
// colour's intensity.
DWORD Shaded(DWORD colour, DWORD grey)
{
    const DWORD r = (colour >> 16) & 0xFF;
    const DWORD g = (colour >> 8) & 0xFF;
    const DWORD b = colour & 0xFF;
    const DWORD intensity = (r + g + b) / 3;
    const DWORD inverse = 255 - intensity;
    return (((r * intensity + inverse * grey) >> 8) << 16) | (((g * intensity + inverse * grey) >> 8) << 8)
        | ((b * intensity + inverse * grey) >> 8);
}

}

// The trap and region highlights of CGameTrigger::Render (0x4CFA60, vtable
// slot 0x4C of 0x84C7BC) and its fill FillPoly (0x4D02C0, slot 0xBC),
// which ours did not have, on AR1000's regions:
// - a trap (type 0) whose outline is drawn for a while (m_drawPoly): red;
// - a found trap picked under the thieving cursor (state 2, icon 36): green;
// - any region while the trigger polygons are shown (CInfGame::field_4AB2,
//   the cheat key Ctrl+4): blue.
// Each is outlined in its COLORREF and, with translucent blits on, filled
// through the FX surface with flag 0x10 in its PackColor. AR1000's traps
// are two-point lines (nothing inside to fill); the travel region is a
// quadrilateral.
TEST_CASE("creature render: AR1000's traps and regions draw their highlight", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    auto find = [&](bool trap) {
        auto accept = [trap](CGameTrigger* p) {
            return (p->m_triggerType == 0) == trap && p->m_nPolygon >= (trap ? 2 : 3) && p->m_rBounding.Width() <= 512
                && p->m_rBounding.Height() <= 512;
        };
        CGameTrigger* p = FindObject<CGameTrigger>(pArea->m_lVertSort, CGameObject::TYPE_TRIGGER, accept);
        return p != NULL ? p : FindObject<CGameTrigger>(pArea->m_lVertSortBack, CGameObject::TYPE_TRIGGER, accept);
    };
    CGameTrigger* pTrap = find(true);
    REQUIRE(pTrap != NULL);
    CGameTrigger* pRegion = find(false);
    REQUIRE(pRegion != NULL);
    // sub_481890 gave each its own bounding rectangle to fill.
    REQUIRE(pTrap->field_630.GetSize() == 1);
    CHECK(pTrap->field_630[0] == &pTrap->m_rBounding);

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    const BOOLEAN translucentBefore = CInfinity::TRANSLUCENT_BLTS_ON;
    const CRect whole(0, 0, scope.Screen().Width(), scope.Screen().Height());
    const WORD trapActivated = pTrap->m_trapActivated;
    const WORD trapDetected = pTrap->m_trapDetected;
    const LONG picked = pArea->m_iPicked;
    const SHORT state = pGame->m_nState;
    const BYTE icon = pGame->m_iconIndex;

    struct Case {
        const char* name;
        CGameTrigger* pTrigger;
        DWORD colour; // 0x00RRGGBB, as the outline is drawn
        bool area; // a polygon with an inside to fill
        bool alone; // no other object changes with it
        std::function<void(bool)> show;
    };
    const Case cases[] = {
        { "trap_drawpoly", pTrap, 0xFF0000, pTrap->m_nPolygon > 2, true, [&](bool on) { pTrap->m_drawPoly = on ? 100 : 0; } },
        { "trap_found", pTrap, 0x00FA00, pTrap->m_nPolygon > 2, true,
            [&](bool on) {
                pTrap->m_trapActivated = on ? 1 : trapActivated;
                pTrap->m_trapDetected = on ? 1 : trapDetected;
                pArea->m_iPicked = on ? pTrap->GetId() : picked;
                pGame->m_nState = on ? 2 : state;
                pGame->m_iconIndex = on ? 36 : icon;
            } },
        { "region_shown", pRegion, 0x0000FF, true, false, [&](bool on) { pGame->field_4AB2 = on ? 1 : 0; } },
    };
    for (const Case& c : cases) {
        INFO(c.name);
        scope.CentreOn(c.pTrigger->m_rBounding.CenterPoint());
        const CRect rScreen = scope.ToScreen(c.pTrigger->m_rBounding);
        CRect rAround(rScreen);
        rAround.InflateRect(1, 1);
        INFO("bounds on screen " << rScreen.left << "," << rScreen.top << " - " << rScreen.right << "," << rScreen.bottom);

        // The fill alone on grey.
        const std::vector<DWORD> grey = scope.Back(true, 0x808080);
        c.pTrigger->FillPoly(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK, scope.VidMode()->PackColor(RGB(c.colour >> 16, (c.colour >> 8) & 0xFF, c.colour & 0xFF)));
        const std::vector<DWORD> greyFilled = scope.Back();
        const Difference shaded = Compare(grey, greyFilled, scope.Screen(), rScreen, Shaded(c.colour, 0x80));
        INFO("fill on grey: changed " << shaded.changed << ", shaded " << shaded.exact);
        if (c.area) {
            CHECK(shaded.changed > 20);
        }
        CHECK(shaded.exact == shaded.changed);
        CHECK(Compare(grey, greyFilled, scope.Screen(), whole).changed == shaded.changed);

        c.show(false);
        CInfinity::TRANSLUCENT_BLTS_ON = TRUE;
        const std::vector<DWORD> plain = scope.RenderArea(pArea);
        c.show(true);
        CInfinity::TRANSLUCENT_BLTS_ON = FALSE;
        const std::vector<DWORD> outlined = scope.RenderArea(pArea);
        CInfinity::TRANSLUCENT_BLTS_ON = TRUE;
        const std::vector<DWORD> filled = scope.RenderArea(pArea);
        scope.Shot(filled, std::string("ar1000_") + c.name);
        c.show(false);

        const Difference outline = Compare(plain, outlined, scope.Screen(), rAround, c.colour);
        INFO("outline: changed " << outline.changed << ", in its colour " << outline.exact);
        CHECK(outline.exact > 20);
        CHECK(outline.changed <= outline.exact);
        // Shown, every region is outlined: elsewhere only in that colour.
        const Difference everywhere = Compare(plain, outlined, scope.Screen(), whole, c.colour);
        if (c.alone) {
            CHECK(everywhere.changed == outline.changed);
        } else {
            CHECK(everywhere.changed <= everywhere.exact);
        }
        const Difference fill = Compare(outlined, filled, scope.Screen(), rAround);
        INFO("fill in the area: changed " << fill.changed);
        if (c.area) {
            CHECK(fill.changed > 0);
        }
        if (c.alone) {
            CHECK(Compare(outlined, filled, scope.Screen(), whole).changed == fill.changed);
        }
    }
    CInfinity::TRANSLUCENT_BLTS_ON = translucentBefore;
}

// A cow (0xB000, ACOW: 00COW and two other CRE files) of
// CGameAnimationTypeAmbientStatic, whose constructor (0x6A5A10) sets
// m_currentVidCellBase to its base cell (0x6A5D73). Ours left it unset, so
// ChangeDirection at the constructor's end made a garbage pointer the
// current cell and set its sequence: building a cow wrote into memory it
// did not own, and drawing one crashed. Every facing now picks the base
// cell, or the extended one past m_extendDirectionTest without mirrored
// BAMs, with a frame to draw; and the cow draws like the others.
TEST_CASE("creature render: CGameAnimationTypeAmbientStatic builds and draws a cow", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing = 0; facing < 16; facing++) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0xB000, colors, facing));
        auto* pCow = dynamic_cast<CGameAnimationTypeAmbientStatic*>(pAnimation.get());
        REQUIRE(pCow != nullptr);
        CHECK(pCow->m_currentVidCellBase == &pCow->m_g1VidCellBase);
        CHECK(pCow->m_currentVidCellExtend == &pCow->m_g1VidCellExtend);
        const bool extended = !CGameAnimationType::MIRROR_BAM && pCow->m_currentBamDirection > pCow->m_extendDirectionTest;
        CHECK(pCow->m_currentVidCell == (extended ? pCow->m_currentVidCellExtend : pCow->m_currentVidCellBase));
        CSize frameSize;
        CHECK(pCow->m_currentVidCell->GetCurrentFrameSize(frameSize, FALSE));
        if (facing == 0 || facing == 12) {
            CheckRender(scope, pCow, CPoint(320, 300), "cow_" + std::to_string(facing));
        }
    }
}

// A wyvern (0xA000, MWYV in three groups of BAMs: 00SWYV, 50WYV, 61WYVERN
// and eight other CRE files) of CGameAnimationTypeMonsterLarge16, whose
// constructor (0x6B23B0) sets m_currentVidCellBase to its first base cell
// (0x6B26F8). Ours left it unset, as AmbientStatic's, so the constructor's
// ChangeDirection made whatever the heap held the current cell. Every
// facing now picks the base cell, or the extended one past
// m_extendDirectionTest without mirrored BAMs, with a frame to draw.
TEST_CASE("creature render: CGameAnimationTypeMonsterLarge16 builds and draws a wyvern", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing = 0; facing < 16; facing++) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0xA000, colors, facing));
        auto* pWyvern = dynamic_cast<CGameAnimationTypeMonsterLarge16*>(pAnimation.get());
        REQUIRE(pWyvern != nullptr);
        CHECK(pWyvern->m_currentVidCellBase == &pWyvern->m_g1VidCellBase);
        CHECK(pWyvern->m_currentVidCellExtend == &pWyvern->m_g1VidCellExtend);
        const bool extended = !CGameAnimationType::MIRROR_BAM && pWyvern->m_currentBamDirection > pWyvern->m_extendDirectionTest;
        CHECK(pWyvern->m_currentVidCell == (extended ? pWyvern->m_currentVidCellExtend : pWyvern->m_currentVidCellBase));
        CSize frameSize;
        CHECK(pWyvern->m_currentVidCell->GetCurrentFrameSize(frameSize, FALSE));
        if (facing == 0 || facing == 12) {
            CheckRender(scope, pWyvern, CPoint(320, 330), "wyvern16_" + std::to_string(facing));
        }
    }
}

// CGameTrigger::IsOver (0x4CEBE0) is FALSE outside the region's bounding
// rectangle (`PtInRect`, then `jne` past the early return at 0x4CEC53) and
// tests the polygon inside it. Ours had the test the wrong way round, so no
// region could ever be picked: in walk_and_talk the mouse rests on the
// opened AR1000_Door5, where the original picks the travel region To1005
// (to AR1005) and shows its cursor, 42 (CGameTrigger's constructor turns
// the file's door cursor 30 into 42), and ours showed the walk cursor, 4.
TEST_CASE("trigger: a travel region is picked inside its bounds", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->SelectCharacter(world.pCharacter->GetId(), FALSE);
    REQUIRE(pGame->GetGroup()->IsPartyLeader());

    // The mouse's world point at walk_and_talk's door_open checkpoint.
    const CPoint ptMouse(845, 900);
    auto accept = [&](CGameTrigger* p) { return p->m_triggerType == 2 && p->m_rBounding.PtInRect(ptMouse); };
    CGameTrigger* pRegion = FindObject<CGameTrigger>(pArea->m_lVertSort, CGameObject::TYPE_TRIGGER, accept);
    if (pRegion == NULL) {
        pRegion = FindObject<CGameTrigger>(pArea->m_lVertSortBack, CGameObject::TYPE_TRIGGER, accept);
    }
    REQUIRE(pRegion != NULL);
    CHECK(pRegion->m_cursorType == 42);

    CHECK(pRegion->IsOver(ptMouse));
    CHECK(pRegion->IsOver(pRegion->m_rBounding.CenterPoint()));
    CHECK_FALSE(pRegion->IsOver(CPoint(pRegion->m_rBounding.right + 4, pRegion->m_rBounding.CenterPoint().y)));
    CHECK_FALSE(pRegion->IsOver(CPoint(pRegion->m_rBounding.left - 4, pRegion->m_rBounding.top - 4)));

    // Only the party leader picks it.
    pGame->UnselectAll();
    CHECK_FALSE(pRegion->IsOver(ptMouse));
}

// CSnowStorm::Render (0x558310) colours each flake with the snow colour it
// is given scaled by CSnowFlake::SNOWFLAKE_BASE_COLOR (230, 230, 230) / 256,
// channel by channel (0x55837D-0x5583C4). Ours multiplied without the
// shift and kept the low byte, so white light drew the flakes nearly black
// (26, 26, 26) where the original draws them (229, 229, 229); under
// AR1000's light both draw (202, 202, 202) now.
TEST_CASE("weather: snow flakes are drawn in the scaled snow colour", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(world.pCharacter->GetPos());
    CInfinity* pInfinity = pArea->GetInfinity();
    const CRect rClip(pInfinity->nCurrentX,
        pInfinity->nCurrentY,
        pInfinity->nCurrentX + pInfinity->rViewPort.Width(),
        pInfinity->nCurrentY + pInfinity->rViewPort.Height());

    CSnowStorm storm;
    storm.GenerateFlakes(CPoint(pInfinity->nCurrentX, pInfinity->nCurrentY), pInfinity->rViewPort);
    REQUIRE(storm.IsInitialized());
    storm.m_nCurrentDensity = 1;
    // One dot, no tail, at (90, 60) in the viewport: the render puts a flake
    // at x >> 10, (3 * y / 4 - z) >> 10.
    CSnowFlake& flake = storm.m_pSnowFlakes[0];
    flake.m_pos.x = (rClip.left + 90) << CParticle::RESOLUTION_INC;
    flake.m_pos.y = (4 * (rClip.top + 60) / 3) << CParticle::RESOLUTION_INC;
    flake.m_pos.z = (3 * flake.m_pos.y / 4 - ((rClip.top + 60) << CParticle::RESOLUTION_INC));
    flake.m_nTailLength = 0;
    flake.m_nTimeStamp = 0;

    struct Case {
        COLORREF rgbColor;
        DWORD expected; // 0x00RRGGBB
    };
    for (const Case& c : { Case { RGB(255, 255, 255), 0xE5E5E5 }, Case { RGB(225, 225, 225), 0xCACACA }, Case { RGB(100, 150, 200), 0x5986B3 } }) {
        INFO("colour " << std::hex << c.rgbColor);
        scope.Back(true, 0);
        storm.Render(scope.VidMode(), 0, rClip, c.rgbColor);
        const std::vector<DWORD> pixels = scope.Back();
        const int width = scope.Screen().Width();
        int drawn = 0;
        for (DWORD pixel : pixels) {
            drawn += (pixel & 0xFFFFFF) != 0;
        }
        CHECK(drawn == 1);
        CHECK((pixels[60 * width + 90] & 0xFFFFFF) == c.expected);
    }
}

// CGameArea::SetDawn (0x478490) ends by passing the dawn on to its
// infinity's SetDawn (the call at 0x478AAB), in an outdoor area. Ours
// called SetDusk, which lights the world for dusk: at intensity 200 a light
// of (255, 230, 230) where dawn gives (222, 230, 255).
TEST_CASE("area: SetDawn lights an outdoor area for dawn", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    CInfinity* pInfinity = pArea->GetInfinity();
    REQUIRE((pInfinity->m_areaType & 0x2) != 0);
    REQUIRE((pInfinity->m_areaType & 0x40) == 0);

    pArea->SetDawn(200, FALSE);
    // n = 400 & 0xFF = 144: CInfinity::SetDawn's constants (0x5D19AB-0x5D1A26).
    CHECK(pInfinity->m_dayLightIntensity == 200);
    CHECK(pInfinity->m_rgbTimeOfDayGlobalLighting == RGB(0xB4 + (144 * 0x4B >> 8), 0xC8 + (144 * 0x37 >> 8), 0xFF));
    CHECK(pInfinity->m_rgbTimeOfDayRainColor == RGB(0x5A + (144 * 0x14 >> 8), 0x5A + (144 * 0x14 >> 8), 0x64 + (144 * 10 >> 8)));
}

// CGameTrigger::IsOverActivate (0x4CECB0) is FALSE outside the region's
// bounding rectangle (`jne` past the return at 0x4CECCE), then tests the
// polygon. Ours had the rectangle test the wrong way round, as IsOver had,
// so a creature walking into a trap or a travel region never set it off
// (CGameTrigger::DoAIUpdate 0x4CD220 asks this of each sprite's position).
TEST_CASE("trigger: a creature inside a travel region sets it off", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    // AR1000's To1005, inside the house door (its bounds 825,875-866,928).
    const CPoint ptInside(845, 900);
    auto accept = [&](CGameTrigger* p) { return p->m_triggerType == 2 && p->m_rBounding.PtInRect(ptInside); };
    CGameTrigger* pRegion = FindObject<CGameTrigger>(pArea->m_lVertSort, CGameObject::TYPE_TRIGGER, accept);
    if (pRegion == NULL) {
        pRegion = FindObject<CGameTrigger>(pArea->m_lVertSortBack, CGameObject::TYPE_TRIGGER, accept);
    }
    REQUIRE(pRegion != NULL);
    REQUIRE(pRegion->m_pPolygon != NULL);

    CHECK(pRegion->IsOverActivate(ptInside));
    CHECK(pRegion->IsOverActivate(pRegion->m_rBounding.CenterPoint()));
    CHECK_FALSE(pRegion->IsOverActivate(CPoint(pRegion->m_rBounding.right + 4, pRegion->m_rBounding.CenterPoint().y)));
    CHECK_FALSE(pRegion->IsOverActivate(CPoint(pRegion->m_rBounding.left - 4, pRegion->m_rBounding.top - 4)));
    // Inside the rectangle the polygon decides.
    int inside = 0;
    int outside = 0;
    for (int y = pRegion->m_rBounding.top; y < pRegion->m_rBounding.bottom; y += 3) {
        for (int x = pRegion->m_rBounding.left; x < pRegion->m_rBounding.right; x += 3) {
            const CPoint pt(x, y);
            const bool expected = CVidPoly::IsPtInPoly(pRegion->m_pPolygon, pRegion->m_nPolygon, pt) != FALSE;
            CHECK((pRegion->IsOverActivate(pt) != FALSE) == expected);
            (expected ? inside : outside)++;
        }
    }
    CHECK(inside > 0);
}

// A pile (container type 4) with no bounding rectangle in the area file gets
// its search square: left and top rounded down to the 16x12 grid, right and
// bottom the square's last pixel, (pos / 16 + 1) * 16 - 1 and
// (pos / 12 + 1) * 12 - 1 (0x47C17A-0x47C1C2 of the constructor 0x47BFA0).
// Its walk-to point, which is also its position, is the rectangle's middle.
// Ours computed 16 * (pos / 17) - 1 and 12 * (pos / 13) - 1, so AR1000's
// goblin axe (file position 490,956) stood at 463,911 and was drawn 27 and
// 45 pixels off; it is at 487,953 as in the original's shots.
TEST_CASE("container: a pile without bounds gets its search square", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    auto accept = [](CGameContainer* p) { return p->m_containerType == 4 && p->m_rBounding.left == 480 && p->m_rBounding.top == 948; };
    CGameContainer* pPile = FindObject<CGameContainer>(pArea->m_lVertSortBack, CGameObject::TYPE_CONTAINER, accept);
    if (pPile == NULL) {
        pPile = FindObject<CGameContainer>(pArea->m_lVertSort, CGameObject::TYPE_CONTAINER, accept);
    }
    REQUIRE(pPile != NULL);
    CHECK(pPile->m_rBounding == CRect(480, 948, 496, 960));
    CHECK(pPile->m_ptWalkToUse == CPoint(487, 953));
    CHECK(pPile->GetPos() == CPoint(487, 953));
}

// The constructors of CGameTrigger (0x4CCAB0), CGameDoor (0x485AC0) and
// CGameContainer (0x47BFA0) copy each polygon from the area file and drop
// a point only when it lies on the line through its two neighbours: the
// same x, the same y, or equal slopes 1000 * dy / dx (`cmp eax, edi; jne`
// keeps it, 0x4CCE1E-0x4CCE20, 0x486031, 0x4861D0, 0x47C45F). Ours had the
// slope test the wrong way round and dropped every point between two
// different slopes: AR1000's Half-Built Ship kept 2 of its 35 points and
// every door's open polygon 2 of 3. The counts expected here are those the
// original saved for AR1000 (its Probe Save, AR1000.ARE in ICEWIND2.SAV of
// the crossload run 20261016-171655): every point, except one of Barrel
// Three's ten, which lies on a straight line.
TEST_CASE("area objects: AR1000's polygons keep the original's points", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;

    enum Kind { kTrigger,
        kDoorOpen,
        kDoorClosed,
        kContainer };
    struct Expected {
        Kind kind;
        const char* name;
        int points;
    };
    const Expected expected[] = {
        { kTrigger, "Door_Hint_Trap_00", 4 },
        { kTrigger, "Door_Hint_Trap_01", 4 },
        { kTrigger, "To1001", 4 },
        { kTrigger, "To1002", 4 },
        { kTrigger, "To1003", 4 },
        { kTrigger, "To1004", 4 },
        { kTrigger, "To1005", 4 },
        { kTrigger, "To1006", 4 },
        { kTrigger, "To1100", 4 },
        { kTrigger, "Info Text for Cranes", 42 },
        { kTrigger, "Wicked Wench", 13 },
        { kTrigger, "Half-Built Ship", 35 },
        { kTrigger, "Life Raft", 11 },
        { kTrigger, "Gutboy's Regrets", 23 },
        { kTrigger, "Light House", 4 },
        { kTrigger, "Dualdon's Wake", 22 },
        { kTrigger, "Salty Dog Tavern", 4 },
        { kDoorOpen, "AR1000_Door4", 3 },
        { kDoorClosed, "AR1000_Door4", 4 },
        { kDoorOpen, "AR1000_Door3", 3 },
        { kDoorClosed, "AR1000_Door3", 4 },
        { kDoorOpen, "AR1000_Door6", 3 },
        { kDoorClosed, "AR1000_Door6", 4 },
        { kDoorOpen, "AR1000_Door5", 3 },
        { kDoorClosed, "AR1000_Door5", 4 },
        { kDoorOpen, "AR1000_Door2", 3 },
        { kDoorClosed, "AR1000_Door2", 4 },
        { kDoorOpen, "AR1000_Door1", 3 },
        { kDoorClosed, "AR1000_Door1", 4 },
        { kContainer, "Goblin Shield", 0 },
        { kContainer, "Goblin Axe", 0 },
        { kContainer, "Goblin Short Sword", 0 },
        { kContainer, "Goblin Short Bow", 0 },
        { kContainer, "Goblin Arrows", 0 },
        { kContainer, "Soldier Helmet", 0 },
        { kContainer, "Soldier Spear", 0 },
        { kContainer, "Soldier Armor", 0 },
        { kContainer, "Sailor Club", 0 },
        { kContainer, "Goblin Axe", 0 },
        { kContainer, "Goblin Shield", 0 },
        { kContainer, "Goblin Sword", 0 },
        { kContainer, "Goblin Axe", 0 },
        { kContainer, "Soldier Spear", 0 },
        { kContainer, "Soldier Helmet", 0 },
        { kContainer, "Soldier Armor", 0 },
        { kContainer, "Throwing Axe", 0 },
        { kContainer, "Throwing Axe", 0 },
        { kContainer, "Barrel One", 10 },
        { kContainer, "Barrel Two", 9 },
        { kContainer, "Barrel Three", 9 },
        { kContainer, "Barrel Four", 14 },
        { kContainer, "Barrel Five", 11 },
        { kContainer, "Dingy Barrel", 7 },
        { kContainer, "Townsperson Axe", 0 },
    };

    // (kind, script name) -> the point counts of the objects of that name.
    std::map<std::pair<int, std::string>, std::multiset<int>> found;
    std::set<LONG> seen;
    CGameObjectArray* pObjects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (CTypedPtrList<CPtrList, int*>* pList : { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight, &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd }) {
        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pList->GetNext(pos)));
            CGameObject* pObject;
            if (!seen.insert(id).second
                || pObjects->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
                continue;
            }
            pObjects->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            if (pObject->GetObjectType() == CGameObject::TYPE_TRIGGER) {
                auto* p = static_cast<CGameTrigger*>(pObject);
                found[{ kTrigger, std::string(p->m_scriptName, strnlen(p->m_scriptName, SCRIPTNAME_SIZE)) }].insert(p->m_nPolygon);
            } else if (pObject->GetObjectType() == CGameObject::TYPE_DOOR) {
                auto* p = static_cast<CGameDoor*>(pObject);
                const std::string name(p->m_scriptName, strnlen(p->m_scriptName, SCRIPTNAME_SIZE));
                found[{ kDoorOpen, name }].insert(p->m_nOpenPolygon);
                found[{ kDoorClosed, name }].insert(p->m_nClosedPolygon);
            } else if (pObject->GetObjectType() == CGameObject::TYPE_CONTAINER) {
                auto* p = static_cast<CGameContainer*>(pObject);
                found[{ kContainer, std::string(p->m_scriptName, strnlen(p->m_scriptName, SCRIPTNAME_SIZE)) }].insert(p->m_nPolygon);
            }
        }
    }

    std::map<std::pair<int, std::string>, std::multiset<int>> wanted;
    int total = 0;
    for (const Expected& e : expected) {
        wanted[{ e.kind, e.name }].insert(e.points);
        total += e.points;
    }
    CHECK(total == 292);
    for (const auto& [key, counts] : wanted) {
        INFO("kind " << key.first << " " << key.second);
        auto it = found.find(key);
        REQUIRE(it != found.end());
        CHECK(it->second == counts);
    }
    int foundTotal = 0;
    for (const auto& [key, counts] : found) {
        for (int n : counts) {
            foundTotal += n;
        }
    }
    CHECK(foundTotal == total);
}

// CGameDoor's constructor (0x485AC0) takes the open and closed bounding
// rectangles from the area file with right and bottom one past the file's
// last pixel (`inc` at 0x485CA6, 0x485CB3, 0x485CD6 and 0x485CE1), and
// Marshal writes them back less one, so a save keeps the file's rectangles.
// Ours took them as they are, so a door was picked one pixel short on the
// right and the bottom and each save shrank them by a pixel. The rectangles
// expected here are those the original saved for AR1000 (the save of the
// test above), plus one on the right and the bottom.
TEST_CASE("door: its bounding rectangles end one past the file's", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    struct Expected {
        const char* name;
        CRect open; // left, top, right, bottom as saved (inclusive)
        CRect closed;
    };
    const Expected expected[] = {
        { "AR1000_Door4", CRect(3175, 4, 3182, 12), CRect(766, 2192, 815, 2253) },
        { "AR1000_Door3", CRect(3170, 6, 3177, 12), CRect(892, 1764, 938, 1822) },
        { "AR1000_Door6", CRect(3180, 4, 3188, 12), CRect(259, 1649, 306, 1713) },
        { "AR1000_Door5", CRect(3180, 3, 3188, 8), CRect(823, 870, 868, 930) },
        { "AR1000_Door2", CRect(3179, 2, 3187, 10), CRect(2252, 455, 2503, 601) },
        { "AR1000_Door1", CRect(3174, 3, 3183, 13), CRect(958, 486, 1005, 540) },
    };
    for (const Expected& e : expected) {
        INFO(e.name);
        auto accept = [&](CGameDoor* p) { return strncmp(p->m_scriptName, e.name, SCRIPTNAME_SIZE) == 0; };
        CGameDoor* pDoor = FindObject<CGameDoor>(pArea->m_lVertSort, CGameObject::TYPE_DOOR, accept);
        if (pDoor == NULL) {
            pDoor = FindObject<CGameDoor>(pArea->m_lVertSortBack, CGameObject::TYPE_DOOR, accept);
        }
        REQUIRE(pDoor != NULL);
        CHECK(pDoor->m_rOpenBounding == CRect(e.open.left, e.open.top, e.open.right + 1, e.open.bottom + 1));
        CHECK(pDoor->m_rClosedBounding == CRect(e.closed.left, e.closed.top, e.closed.right + 1, e.closed.bottom + 1));
    }
}

// CGameAnimationTypeCharacter::Render (0x6CD400), facing 3 to 7 (south
// west round to north east, and their mirrors): the weapon is drawn behind
// the body, before it, unless the weapon's item has flag 0x80 (field_1434,
// set by EquipWeapon 0x6C8390), in which case it is drawn last, in front
// (the tests of +0x1434 at 0x6CD6AD and 0x6CD7B2). Ours drew it both before
// and after the body, so a spear held across the body showed its shaft over
// it: the crew in world_entry's world shot.
//
// The "weapon" here is a second cell on the body's own BAM at another
// frame, so the two overlap: behind the body, every pixel the body draws
// keeps the body's colour; in front, the weapon replaces some of them.
TEST_CASE("creature render: a character's weapon goes behind its body unless its item puts it in front", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing : { 3, 4, 6, 12 }) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x6100, colors, facing));
        auto* pCharacter = dynamic_cast<CGameAnimationTypeCharacter*>(pAnimation.get());
        REQUIRE(pCharacter != nullptr);
        const SHORT nDirection = pCharacter->m_currentBamDirection > pCharacter->m_extendDirectionTest
            ? 16 - pCharacter->m_currentBamDirection
            : pCharacter->m_currentBamDirection;
        REQUIRE(nDirection >= 3);
        REQUIRE(nDirection <= 7);

        CVidCell weapon;
        weapon.SetResRef(pCharacter->m_currentVidCell->cResRef, FALSE, TRUE, TRUE);
        weapon.SequenceSet(pCharacter->m_currentVidCell->m_nCurrentSequence);
        weapon.FrameSet(static_cast<SHORT>(pCharacter->m_currentVidCell->m_nCurrentFrame + 3));
        CVidCell* pWeaponBefore = pCharacter->m_currentVidCellWeapon;
        const BOOLEAN renderWeaponsBefore = pCharacter->m_renderWeapons;

        auto draw = [&](bool weapons, bool front) {
            pCharacter->m_renderWeapons = weapons;
            pCharacter->m_currentVidCellWeapon = weapons ? &weapon : pWeaponBefore;
            pCharacter->field_1434 = front ? 1 : 0;
            scope.Back(true, 0);
            CRect rFx;
            CPoint ptReference;
            pCharacter->CalculateFxRect(rFx, ptReference, 0);
            CRect rBounds;
            const CPoint pos(320, 330);
            pCharacter->CalculateGCBoundsRect(rBounds, pos, ptReference, 0, rFx.Width(), rFx.Height());
            pCharacter->Render(scope.Infinity(), scope.VidMode(), CVIDINF_SURFACE_BACK, rFx, pos, ptReference, 0, RGB(255, 255, 255), rBounds, FALSE, FALSE, 0, 0);
            return scope.Back();
        };
        const std::vector<DWORD> body = draw(false, false);
        const std::vector<DWORD> behind = draw(true, false);
        const std::vector<DWORD> front = draw(true, true);
        pCharacter->m_currentVidCellWeapon = pWeaponBefore;
        pCharacter->m_renderWeapons = renderWeaponsBefore;

        int bodyPixels = 0;
        int hiddenWrong = 0;
        int frontOver = 0;
        int weaponOutside = 0;
        for (size_t i = 0; i < body.size(); i++) {
            if ((body[i] & 0xFFFFFF) != 0) {
                bodyPixels++;
                hiddenWrong += (behind[i] & 0xFFFFFF) != (body[i] & 0xFFFFFF);
                frontOver += (front[i] & 0xFFFFFF) != (body[i] & 0xFFFFFF);
            } else {
                weaponOutside += (behind[i] & 0xFFFFFF) != 0;
            }
        }
        INFO("body " << bodyPixels << ", behind but changed " << hiddenWrong << ", in front over the body " << frontOver << ", weapon around the body " << weaponOutside);
        CHECK(bodyPixels > 100);
        CHECK(hiddenWrong == 0);
        CHECK(frontOver > 20);
        CHECK(weaponOutside > 0);
    }
}

// CGameSprite::Render (0x703700) draws the creature at its position lowered
// by the area's height map: after CalculateFxRect it sets the render point
// to (m_pos.x, m_pos.y + GetHeightOffset(m_pos, m_listType)) (0x70396F-
// 0x703998) and asks GetTintColor at that point plus m_posZ (0x7039CB-
// 0x7039F4). Ours drew it at the clamped position with no height offset,
// so on AR1000's +1 ground every creature stood a pixel too high. Here the
// same wolf is drawn at two points whose height offsets differ: the second
// drawing is the first moved by the difference in
// position plus the offset. (AR1000 is at +1 around the party, so the
// points are two whose offsets differ.)
TEST_CASE("creature render: a creature is drawn at its height offset", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    const CPoint base = world.pCharacter->GetPos();

    // Two points 64 to 160 pixels apart on one row whose height offsets
    // differ, searched from the party's position outwards.
    CPoint pt0(-1, -1);
    CPoint pt1(-1, -1);
    int h0 = 0;
    int h1 = 0;
    for (int r = 0; r < 1200 && pt0.x < 0; r += 12) {
        for (int sy : { 1, -1 }) {
            const int y = base.y + sy * r;
            if (pt0.x >= 0 || y < 200 || y >= pArea->GetInfinity()->nAreaY - 200) {
                continue;
            }
            for (int x = 200; x + 160 < pArea->GetInfinity()->nAreaX - 200 && pt0.x < 0; x += 16) {
                const int a = pArea->GetHeightOffset(CPoint(x, y), CGameObject::LIST_FRONT);
                for (int d = 64; d <= 160; d += 16) {
                    const int b = pArea->GetHeightOffset(CPoint(x + d, y), CGameObject::LIST_FRONT);
                    if (a != b) {
                        pt0 = CPoint(x, y);
                        pt1 = CPoint(x + d, y);
                        h0 = a;
                        h1 = b;
                        break;
                    }
                }
            }
        }
    }
    REQUIRE(pt0.x >= 0);
    INFO("offset " << h0 << " at " << pt0.x << "," << pt0.y << "; offset " << h1 << " at " << pt1.x << "," << pt1.y);

    CGameSprite* pWolf = AddCreature(pArea, "12WOLF", pt0);
    REQUIRE(pWolf != NULL);
    pWolf->m_canBeSeen = CGameObject::VISIBLE_DELAY + 10;
    pWolf->m_baseStats.field_294 = 0;
    pArea->SortLists();

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(CPoint((pt0.x + pt1.x) / 2, (pt0.y + pt1.y) / 2));
    const int width = scope.Screen().Width();
    const int height = scope.Screen().Height();
    auto mask = [&](const CPoint& pt) {
        pWolf->m_pos = pt;
        pWolf->m_baseStats.field_294 = 0;
        const std::vector<DWORD> with = scope.RenderArea(pArea);
        pWolf->m_baseStats.field_294 = 1;
        const std::vector<DWORD> without = scope.RenderArea(pArea);
        pWolf->m_baseStats.field_294 = 0;
        std::vector<bool> changed(with.size());
        for (size_t i = 0; i < with.size(); i++) {
            changed[i] = (with[i] & 0xFFFFFF) != (without[i] & 0xFFFFFF);
        }
        return changed;
    };
    const std::vector<bool> m0 = mask(pt0);
    const std::vector<bool> m1 = mask(pt1);
    auto overlap = [&](int dx, int dy) {
        int n = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int x1 = x + dx;
                const int y1 = y + dy;
                if (m0[y * width + x] && x1 >= 0 && x1 < width && y1 >= 0 && y1 < height && m1[y1 * width + x1]) {
                    n++;
                }
            }
        }
        return n;
    };
    const int n0 = static_cast<int>(std::count(m0.begin(), m0.end(), true));
    const int dx = pt1.x - pt0.x;
    const int dy = pt1.y - pt0.y;
    const int atOffset = overlap(dx, dy + h1 - h0);
    const int withoutOffset = overlap(dx, dy);
    INFO("drawn " << n0 << ", matching at the offset " << atOffset << ", without it " << withoutOffset);
    CHECK(n0 > 300);
    CHECK(atOffset * 20 > n0 * 17);
    CHECK(atOffset > withoutOffset);
}
