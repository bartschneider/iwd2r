#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CGameAnimationType.h"
#include "CGameAnimationTypeMonster.h"
#include "CGameAnimationTypeMonsterAnkheg.h"
#include "CGameAnimationTypeMonsterMulti.h"
#include "CGameAnimationTypeMonsterOld.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameObjectArray.h"
#include "engine_world.h"
#include "render_surfaces.h"

#include <functional>
#include <memory>
#include <string>
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
        CRect rBounds;
        c.pSprite->GetAnimation()->CalculateGCBoundsRect(rBounds, c.pSprite->GetPos(), ptReference, c.pSprite->m_posZ, rFx.Width(), rFx.Height());
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
    // The rectangles the fills walk: the original's CGameAIBase::sub_481890
    // (0x481890) puts the bounding rectangle itself in the array when it is
    // at most 512 x 512, as these are, else 512 x 512 pieces of it. Ours is
    // still a stub (vis-probe's file) and leaves the arrays empty, so the
    // test fills them as the original would.
    REQUIRE(pDoor->m_rClosedBounding.Width() <= 512);
    REQUIRE(pDoor->m_rClosedBounding.Height() <= 512);
    REQUIRE(pContainer->m_rBounding.Width() <= 512);
    REQUIRE(pContainer->m_rBounding.Height() <= 512);
    pDoor->field_6DA.SetSize(0);
    pDoor->field_6DA.Add(&pDoor->m_rClosedBounding);
    pContainer->field_8D6.SetSize(0);
    pContainer->field_8D6.Add(&pContainer->m_rBounding);

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
        CHECK(Compare(outlined, filled, scope.Screen(), whole).changed == fill.changed);
    }
    CInfinity::TRANSLUCENT_BLTS_ON = translucentBefore;
}
