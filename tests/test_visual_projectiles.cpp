#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CGameEffect.h"
#include "CGameObject84D624.h"
#include "CGameObjectArray.h"
#include "CMessage.h"
#include "engine_world.h"
#include "msvc_rand.h"
#include "render_surfaces.h"

#include <memory>
#include <string>
#include <vector>

// The particle object of the sparkles and explosions (CGameObject84D624,
// 0x51AE40) and the projectiles of the Visual Spell Hit effect (opcode 141)
// in AR1000 of a new game: built, updated until they remove themselves, and
// drawn into DirectDraw shim surfaces (tests/render_surfaces.h's
// AreaScope). The rand draws of their constructors, AddToArea and AIUpdate
// are pinned: a replay depends on their count and order. Every engine works
// on a private view of the install (tests/data_view.h).

using namespace engine_world;
using namespace render_surfaces;

namespace {

unsigned int Advance(unsigned int state, int draws)
{
    for (int i = 0; i < draws; ++i) {
        state = state * 214013u + 2531011u;
    }
    return state;
}

// How many rand draws took the generator from before to after (-1 when more
// than 2000).
int Draws(unsigned int before, unsigned int after)
{
    for (int n = 0; n <= 2000; n++) {
        if (Advance(before, n) == after) {
            return n;
        }
    }
    return -1;
}

struct World {
    CGameSprite* pCharacter = NULL;
    CGameArea* pArea = NULL;
};

// A new game in AR1000 with its party member, the area explored and
// visible; made once per process (a second NewGame asserts).
bool NewWorld(World& world, std::string& why)
{
    static World made;
    if (made.pArea != NULL) {
        world = made;
        return true;
    }
    if (DataDir() == nullptr) {
        why = "IWD2_DATA unset";
        return false;
    }
    std::string error;
    if (!StartEngine(error)) {
        why = error;
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
    made = world;
    return true;
}

// The engine starts before the frame pump, which writes to it, and the
// new game after.
#define WORLD_OR_SKIP(world)                  \
    if (DataDir() == nullptr) {               \
        SKIP("[iwd2r-skip] IWD2_DATA unset"); \
    }                                         \
    {                                         \
        std::string error;                    \
        if (!StartEngine(error)) {            \
            SKIP("[iwd2r-skip] " + error);    \
        }                                     \
    }                                         \
    FramePump pump;                           \
    World world;                              \
    {                                         \
        std::string why;                      \
        if (!NewWorld(world, why)) {          \
            SKIP("[iwd2r-skip] " + why);      \
        }                                     \
    }

// Whether the object array still holds the object.
bool Alive(LONG id)
{
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CGameObject* pObject;
    if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
        return false;
    }
    pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    return true;
}

// Runs the object's AIUpdate until it removes itself (at most `limit`
// times); returns how many updates it took, -1 when it stayed.
int UpdateUntilGone(LONG id, int limit)
{
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (int frame = 0; frame < limit; frame++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            return frame;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pObject->AIUpdate();
    }
    return Alive(id) ? -1 : limit;
}

// The objects the area holds in its add lists (what was added since the
// last SortLists).
std::vector<CGameObject*> AddedObjects(CGameArea* pArea)
{
    std::vector<CGameObject*> objects;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd };
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (CTypedPtrList<CPtrList, int*>* pList : lists) {
        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pList->GetNext(pos)));
            CGameObject* pObject;
            if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                objects.push_back(pObject);
            }
        }
    }
    return objects;
}

// What the object's Render alone leaves in the viewport, cleared to black
// first.
std::vector<DWORD> RenderObject(AreaScope& scope, CGameObject* pObject)
{
    scope.Back(true, 0);
    pObject->Render(pObject->GetArea(), scope.VidMode(), CVIDINF_SURFACE_BACK);
    return scope.Back();
}

// The pixels that are not black, inside and outside a screen rectangle.
struct Lit {
    int inside = 0;
    int outside = 0;
};

Lit CountLit(const std::vector<DWORD>& pixels, const CRect& rScreen, const CRect& rIn)
{
    Lit lit;
    for (int y = 0; y < rScreen.Height(); y++) {
        for (int x = 0; x < rScreen.Width(); x++) {
            if ((pixels[y * rScreen.Width() + x] & 0xFFFFFF) == 0) {
                continue;
            }
            if (rIn.PtInRect(CPoint(x, y))) {
                lit.inside++;
            } else {
                lit.outside++;
            }
        }
    }
    return lit;
}

}

// An ice explosion (the death effect's and Sparkle type 1's): its five
// colours are SPRKLCLR.2DA's CSPARKLE_COLORTYPE_ICE row; AddToArea makes
// three streams of 14 particles fanned around the direction from its
// position to its source, two draws each (84), and sets its bounds 50
// around the position. Its updates draw nothing (TYPE_EXPLOSION); the
// slowest particle (gravity 50, rising at 1000) lands on the 65th update
// from a height of 40, and the object removes itself. Its render draws only
// inside its bounds.
TEST_CASE("visual projectiles: an explosion fans out, lands and draws inside its bounds", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x + 60, world.pCharacter->GetPos().y);

    CGameObject84D624* pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_ICE,
        CGameObject84D624::TYPE_EXPLOSION,
        CGameObject84D624::EXPLOSION_PUFFS);
    CHECK(pExplosion->m_rgbColors[0] == RGB(235, 245, 245));
    CHECK(pExplosion->m_rgbColors[1] == RGB(194, 233, 244));
    CHECK(pExplosion->m_rgbColors[2] == RGB(127, 218, 246));
    CHECK(pExplosion->m_rgbColors[3] == RGB(114, 197, 226));
    CHECK(pExplosion->m_rgbColors[4] == RGB(70, 132, 161));
    CHECK(pExplosion->m_wType == CGameObject84D624::TYPE_EXPLOSION);
    CHECK(pExplosion->m_nPuffs == 25);
    CHECK(pExplosion->m_nPuff == 0);
    CHECK(pExplosion->m_nGravity == 50);
    CHECK(pExplosion->m_rBounding.IsRectEmpty());
    CHECK(pExplosion->m_particleList.IsEmpty());
    CHECK(pExplosion->GetId() == CGameObjectArray::INVALID_INDEX);

    // The source due east: the streams go north-east, east and south-east.
    pExplosion->SetSource(CPoint(pos.x + 100, pos.y));
    iwd2r_srand(7);
    pExplosion->AddToArea(pArea, pos, 40, CGameObject::LIST_FRONT);
    CHECK(Draws(7, iwd2r_rand_state()) == 84);
    const LONG id = pExplosion->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pExplosion->GetArea() == pArea);
    CHECK(pExplosion->m_posZ == 40);
    CHECK(pExplosion->m_nFadeHeights[0] == 39);
    CHECK(pExplosion->m_nFadeHeights[1] == 34);
    CHECK(pExplosion->m_nFadeHeights[2] == 30);
    CHECK(pExplosion->m_nFadeHeights[3] == 22);
    CHECK(pExplosion->m_rBounding == CRect(pos.x - 50, pos.y - 40 - 50, pos.x + 50, pos.y - 40 + 50));
    REQUIRE(pExplosion->m_particleList.GetCount() == 42);
    CParticle* pFirst = pExplosion->m_particleList.GetHead();
    CParticle* pMiddle = pExplosion->m_particleList.GetAt(pExplosion->m_particleList.FindIndex(14));
    CParticle* pLast = pExplosion->m_particleList.GetTail();
    CHECK(pFirst->m_vel.x == 700);
    CHECK(pFirst->m_vel.y == -700);
    CHECK(pFirst->m_vel.z == 1000);
    CHECK(pFirst->m_nGravity == 50);
    CHECK(pFirst->m_pos.z == 40 << 10);
    CHECK(pMiddle->m_vel.x == 1000);
    CHECK(pMiddle->m_vel.y == 0);
    CHECK(pMiddle->m_nGravity == 50);
    CHECK(pLast->m_vel.x == 700);
    CHECK(pLast->m_vel.y == 700);
    CHECK(pLast->m_nGravity == 50 + 13 * 10);
    for (POSITION p = pExplosion->m_particleList.GetHeadPosition(); p != NULL;) {
        CParticle* pParticle = pExplosion->m_particleList.GetNext(p);
        // Within 18 of the position less 10 (two draws of 0-9, doubled).
        CHECK(pParticle->m_pos.x >> 10 >= pos.x - 10);
        CHECK(pParticle->m_pos.x >> 10 <= pos.x + 8);
    }

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        const std::vector<DWORD> pixels = RenderObject(scope, pExplosion);
        scope.Shot(pixels, "explosion");
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(pExplosion->m_rBounding));
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    iwd2r_srand(9);
    CHECK(UpdateUntilGone(id, 200) == 65);
    CHECK(Draws(9, iwd2r_rand_state()) == 0);
    CHECK(!Alive(id));
    pArea->SortLists();
}

// The sparkle a projectile trails (TYPE_SPARKLE): 14 particles in a puff,
// two draws each (28); each update draws one number per particle (it may
// turn one back) and lets them fall.
TEST_CASE("visual projectiles: a sparkle puff draws one number per particle each update", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x - 40, world.pCharacter->GetPos().y + 20);

    CGameObject84D624* pSparkle = new CGameObject84D624(2, CGameObject84D624::TYPE_SPARKLE, 25);
    // CSPARKLE_COLORTYPE_BLUE, the table's first row.
    CHECK(pSparkle->m_rgbColors[0] == RGB(235, 241, 245));
    CHECK(pSparkle->m_rgbColors[4] == RGB(70, 92, 161));

    iwd2r_srand(5);
    pSparkle->AddToArea(pArea, pos, 30, CGameObject::LIST_FRONT);
    CHECK(Draws(5, iwd2r_rand_state()) == 28);
    const LONG id = pSparkle->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pSparkle->m_rBounding == CRect(pos.x - 5, pos.y - 5 - 30, pos.x + 4, pos.y + 4));
    REQUIRE(pSparkle->m_particleList.GetCount() == 14);
    CHECK(pSparkle->m_particleList.GetHead()->m_vel.x == 4);
    CHECK(pSparkle->m_particleList.GetHead()->m_nGravity == 50);
    CHECK(pSparkle->m_particleList.GetTail()->m_vel.x == -4);
    CHECK(pSparkle->m_particleList.GetTail()->m_nGravity == 50 + 13 * 10);

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        const std::vector<DWORD> pixels = RenderObject(scope, pSparkle);
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(pSparkle->m_rBounding));
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    iwd2r_srand(6);
    pSparkle->AIUpdate();
    CHECK(Draws(6, iwd2r_rand_state()) == 14);
    CHECK(pSparkle->m_particleList.GetCount() == 14);

    CHECK(UpdateUntilGone(id, 400) > 0);
    CHECK(!Alive(id));
    pArea->SortLists();
}

// The shower (Sparkle type 3, TYPE_SHOWER): a puff of 14 at AddToArea, then
// one more each update, around its position in turn, until it has made
// m_nPuffs; each update also draws one number per particle it has.
TEST_CASE("visual projectiles: a shower adds a puff each update until it has made its puffs", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x + 20, world.pCharacter->GetPos().y - 30);

    CGameObject84D624* pShower = new CGameObject84D624(4, CGameObject84D624::TYPE_SHOWER, 3);
    iwd2r_srand(3);
    pShower->AddToArea(pArea, pos, 30, CGameObject::LIST_FRONT);
    CHECK(Draws(3, iwd2r_rand_state()) == 28);
    const LONG id = pShower->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pShower->m_nPuff == 0);
    CHECK(pShower->m_rBounding == CRect(pos.x - 13, pos.y - 30 - 13, pos.x + 13, pos.y + 13));
    CHECK(pShower->m_particleList.GetCount() == 14);

    const int expected[] = { 14 + 28, 28 + 28, 42 + 28, 56 };
    for (int update = 0; update < 4; update++) {
        INFO("update " << update);
        iwd2r_srand(100 + update);
        pShower->AIUpdate();
        CHECK(Draws(100 + update, iwd2r_rand_state()) == expected[update]);
        CHECK(pShower->m_nPuff == static_cast<DWORD>(update < 3 ? update + 1 : 3));
    }
    CHECK(pShower->m_particleList.GetCount() == 56);

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        const std::vector<DWORD> pixels = RenderObject(scope, pShower);
        const Lit lit = CountLit(pixels, scope.Screen(), scope.ToScreen(pShower->m_rBounding));
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    CHECK(UpdateUntilGone(id, 400) > 0);
    CHECK(!Alive(id));
    pArea->SortLists();
}

namespace {

// The particle objects the area holds in its add lists.
std::vector<CGameObject84D624*> AddedParticles(CGameArea* pArea)
{
    std::vector<CGameObject84D624*> particles;
    for (CGameObject* pObject : AddedObjects(pArea)) {
        if (CGameObject84D624* pParticles = dynamic_cast<CGameObject84D624*>(pObject)) {
            particles.push_back(pParticles);
        }
    }
    return particles;
}

std::unique_ptr<CGameEffect> DecodeAt(WORD effectID, DWORD dwFlags, LONG effectAmount, const CPoint& source, LONG sourceID, const CPoint& target)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, effectID);
    effect.dwFlags = dwFlags;
    effect.effectAmount = effectAmount;
    return std::unique_ptr<CGameEffect>(CGameEffect::DecodeEffect(&effect, source, sourceID, target));
}

}

// The death effect's shatters with gore (dwFlags 0x80, ice, and 0x40,
// stone; CGameEffectDeath::ApplyEffect 0x4AB510 at 0x4AC69B and 0x4AC8DC):
// two explosions at the creature on the area's back list, one flying away
// from the effect's source and one from its mirror (the source negated), as
// high as half the creature's fx rect, their particles' gravity starting at
// 100. Each explosion draws 84 numbers.
TEST_CASE("visual projectiles: a shattering death throws two explosions", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameOptions* pOptions = g_pBaldurChitin->GetObjectGame()->GetOptions();
    const DWORD nGore = pOptions->m_nGore;
    pOptions->m_nGore = 1;

    // The stone shatter sets the die sequence first (CGameSprite::
    // SetSequence draws one number), as the original does at 0x4AC907.
    struct Shatter {
        DWORD dwFlags;
        COLORREF rgbStart;
        int draws;
    };
    const Shatter shatters[] = {
        { 0x80, RGB(235, 245, 245), 168 },
        { 0x40, RGB(150, 150, 150), 1 + 168 },
    };
    int n = 0;
    for (const Shatter& shatter : shatters) {
        INFO("death flags " << shatter.dwFlags);
        const CPoint pos(world.pCharacter->GetPos().x + 80 + 40 * n++, world.pCharacter->GetPos().y + 30);
        CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
        REQUIRE(pGoblin != NULL);
        pArea->SortLists();

        const CPoint source(pos.x - 30, pos.y - 10);
        std::unique_ptr<CGameEffect> pDeath = DecodeAt(CGAMEEFFECT_DEATH, shatter.dwFlags, 0, source, world.pCharacter->GetId(), pos);
        REQUIRE(pDeath != nullptr);
        iwd2r_srand(17);
        pDeath->ApplyEffect(pGoblin);
        INFO("draws " << Draws(17, iwd2r_rand_state()));
        CHECK(Draws(17, iwd2r_rand_state()) == shatter.draws);

        std::vector<CGameObject84D624*> explosions = AddedParticles(pArea);
        REQUIRE(explosions.size() == 2);
        CRect rFx;
        CPoint ptReference;
        pGoblin->GetAnimation()->CalculateFxRect(rFx, ptReference, pGoblin->m_posZ);
        for (CGameObject84D624* pExplosion : explosions) {
            CHECK(pExplosion->m_wType == CGameObject84D624::TYPE_EXPLOSION);
            CHECK(pExplosion->m_rgbColors[0] == shatter.rgbStart);
            CHECK(pExplosion->m_nGravity == 100);
            CHECK(pExplosion->m_particleList.GetCount() == 42);
            CHECK(pExplosion->GetPos() == pos);
            CHECK(pExplosion->m_posZ == static_cast<SHORT>(rFx.Height() * 5 / 10));
            CHECK(pExplosion->GetVertListType() == CGameObject::LIST_BACK);
        }
        CHECK(explosions[0]->m_ptSource == source);
        CHECK(explosions[1]->m_ptSource == CPoint(-source.x, -source.y));

        for (CGameObject84D624* pExplosion : explosions) {
            CHECK(UpdateUntilGone(pExplosion->GetId(), 400) > 0);
        }
        pArea->SortLists();
    }

    pOptions->m_nGore = nGore;
}

// The Sparkle effect (opcode 41, CGameEffectSparkle::ApplyEffect 0x4B1700)
// at a creature, its source unset (-1, -1): type 1 is an explosion of the
// effect amount's colour at the creature's position one lower, as high as
// 8/10 of its fx rect, on the front list (84 draws); type 3 a shower that
// makes a puff for each tick left of the effect's duration (28 draws);
// other types add nothing. The effect is done after one application.
TEST_CASE("visual projectiles: the Sparkle effect adds an explosion or a shower", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    CRect rFx;
    CPoint ptReference;
    pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, pSprite->m_posZ);
    const CPoint pos(pSprite->GetPos().x, pSprite->GetPos().y + 1);

    std::unique_ptr<CGameEffect> pSparkle = DecodeAt(CGAMEEFFECT_SPARKLE, 1, 7, CPoint(-1, -1), pSprite->GetId(), pSprite->GetPos());
    REQUIRE(pSparkle != nullptr);
    iwd2r_srand(23);
    pSparkle->ApplyEffect(pSprite);
    CHECK(Draws(23, iwd2r_rand_state()) == 84);
    CHECK(pSparkle->m_done == TRUE);
    std::vector<CGameObject84D624*> added = AddedParticles(pArea);
    REQUIRE(added.size() == 1);
    CHECK(added[0]->m_wType == CGameObject84D624::TYPE_EXPLOSION);
    // CSPARKLE_COLORTYPE_RED.
    CHECK(added[0]->m_rgbColors[0] == RGB(248, 240, 241));
    CHECK(added[0]->m_ptSource == CPoint(-1, -1));
    CHECK(added[0]->GetPos() == pos);
    CHECK(added[0]->m_posZ == static_cast<SHORT>(rFx.Height() * 8 / 10));
    CHECK(added[0]->GetVertListType() == CGameObject::LIST_FRONT);
    CHECK(UpdateUntilGone(added[0]->GetId(), 400) > 0);
    pArea->SortLists();

    pSparkle = DecodeAt(CGAMEEFFECT_SPARKLE, 3, 12, CPoint(-1, -1), pSprite->GetId(), pSprite->GetPos());
    REQUIRE(pSparkle != nullptr);
    pSparkle->m_duration = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime + 5;
    iwd2r_srand(29);
    pSparkle->ApplyEffect(pSprite);
    CHECK(Draws(29, iwd2r_rand_state()) == 28);
    added = AddedParticles(pArea);
    REQUIRE(added.size() == 1);
    CHECK(added[0]->m_wType == CGameObject84D624::TYPE_SHOWER);
    CHECK(added[0]->m_nPuffs == 5);
    // CSPARKLE_COLORTYPE_ORANGE.
    CHECK(added[0]->m_rgbColors[0] == RGB(255, 239, 232));
    CHECK(UpdateUntilGone(added[0]->GetId(), 400) > 0);
    pArea->SortLists();

    pSparkle = DecodeAt(CGAMEEFFECT_SPARKLE, 2, 7, CPoint(-1, -1), pSprite->GetId(), pSprite->GetPos());
    REQUIRE(pSparkle != nullptr);
    iwd2r_srand(31);
    pSparkle->ApplyEffect(pSprite);
    CHECK(Draws(31, iwd2r_rand_state()) == 0);
    CHECK(AddedParticles(pArea).empty());
    CHECK(pSparkle->m_done == TRUE);
}
