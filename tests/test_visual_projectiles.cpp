#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CGameEffect.h"
#include "CGameObject84D624.h"
#include "CGameObjectArray.h"
#include "CMessage.h"
#include "CProjectile84DFA4.h"
#include "CProjectile84E940.h"
#include "CProjectile84E9C0.h"
#include "CProjectileBAM.h"
#include "CProjectileShaft.h"
#include "CProjectileSingleTarget.h"
#include "CProjectileSpellHit.h"
#include "CSpell.h"
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

namespace {

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The one object of this class the area holds in its add lists, or NULL.
template <class T>
T* AddedOne(CGameArea* pArea)
{
    std::vector<CGameObject*> objects = AddedObjects(pArea);
    if (objects.size() != 1) {
        return NULL;
    }
    return dynamic_cast<T*>(objects.front());
}

// The pixels the render lit, all of them, and those outside `rIn` (a world
// rectangle).
Lit RenderLit(AreaScope& scope, CGameObject* pObject, const CRect& rIn, const std::string& shot)
{
    const std::vector<DWORD> pixels = RenderObject(scope, pObject);
    scope.Shot(pixels, shot);
    return CountLit(pixels, scope.Screen(), scope.ToScreen(rIn));
}

}

// The burst (CProjectile84DFA4, SPBOOM, 0x530300): its colour range set
// (0x52B150), PlayAt (vtable 0x88, 0x530510) puts it at a point without
// firing it; CProjectileBAM::Render (0x52B190) draws its frame around the
// point; its updates (0x5306F0) play the sequence once, then it removes
// itself. No rand draws.
TEST_CASE("visual projectiles: the burst plays once at a point and removes itself", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    pArea->SortLists();
    const CPoint pos(world.pCharacter->GetPos().x + 50, world.pCharacter->GetPos().y + 40);

    CProjectile84DFA4* pBurst = new CProjectile84DFA4();
    CHECK(pBurst->m_animation->m_nCurrentSequence == 0);
    CHECK(pBurst->field_1D8 == 1);
    CHECK(pBurst->field_1DE == TRUE);
    CHECK(pBurst->field_1BE == FALSE);
    CHECK(pBurst->field_1D4 == 0);
    CHECK(pBurst->m_fireSoundRef == "");
    pBurst->SetColorRange(0, 0x41);

    iwd2r_srand(41);
    pBurst->PlayAt(pArea, pos);
    CHECK(Draws(41, iwd2r_rand_state()) == 0);
    const LONG id = pBurst->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pBurst->GetPos() == pos);
    CHECK(pBurst->GetArea() == pArea);
    CHECK(AddedOne<CProjectile84DFA4>(pArea) == pBurst);

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        CRect rFx;
        CPoint ptReference;
        pBurst->CalculateFxRect(rFx, ptReference, NULL);
        const CRect rBounds(pos.x - ptReference.x, pos.y - ptReference.y, pos.x - ptReference.x + rFx.Width(), pos.y - ptReference.y + rFx.Height());
        const Lit lit = RenderLit(scope, pBurst, rBounds, "burst");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    const int nLength = pBurst->m_animation->GetSequenceLength(0, FALSE);
    REQUIRE(nLength > 1);
    iwd2r_srand(42);
    CHECK(UpdateUntilGone(id, 400) == nLength - 1);
    CHECK(Draws(42, iwd2r_rand_state()) == 0);
    pArea->SortLists();
    ClearMessages();
}

// The shaft (CProjectileShaft, 0x5348C0): fired at a sprite (0x535100) it
// delivers its effects at once (an empty group here), stands 13 below the
// sprite, draws its sequences stacked upwards (0x534DD0) and goes when its
// duration runs out (0x534C10). It draws a number each update only with a
// random frame.
TEST_CASE("visual projectiles: the shaft stands on its target for its duration", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();
    const CPoint pos = pSprite->GetPos();

    CProjectileShaft* pShaft = new CProjectileShaft(CString("FlmStrk"), CString(""), 0xFF, FALSE, FALSE, 30);
    CHECK(pShaft->m_nDuration == 30);
    CHECK(pShaft->m_bTransparent == FALSE);
    CHECK(pShaft->m_bRandomFrame == FALSE);
    CHECK(pShaft->m_bHasHeight == FALSE);
    CHECK(pShaft->DetermineHeight(pSprite) == 0);
    REQUIRE(pShaft->m_animation.GetNumberSequences(FALSE) > 1);

    iwd2r_srand(43);
    pShaft->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pos, 0, 0);
    CHECK(Draws(43, iwd2r_rand_state()) == 0);
    const LONG id = pShaft->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pShaft->GetPos() == CPoint(pos.x, pos.y + 13));
    CHECK(AddedOne<CProjectileShaft>(pArea) == pShaft);
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageGroup* pGroup = dynamic_cast<CMessageGroup*>(handler.m_messageList.GetHead());
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->m_messages.empty());
    ClearMessages();

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(CPoint(pos.x, pos.y - 100));
        // A column over the shaft's foot.
        const CRect rColumn(pos.x - 150, pos.y - 600, pos.x + 150, pos.y + 13 + 60);
        const Lit lit = RenderLit(scope, pShaft, rColumn, "shaft");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    iwd2r_srand(44);
    CHECK(UpdateUntilGone(id, 100) == 30);
    CHECK(Draws(44, iwd2r_rand_state()) == 0);
    pArea->SortLists();

    // A random frame each update: one draw per update but the last.
    pShaft = new CProjectileShaft(CString("HLYMITE"), CString(""), 0xFF, TRUE, TRUE, 5);
    pShaft->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pos, 0, 0);
    REQUIRE(pShaft->m_animation.GetSequenceLength(0, FALSE) > 0);
    iwd2r_srand(45);
    CHECK(UpdateUntilGone(pShaft->GetId(), 100) == 5);
    CHECK(Draws(45, iwd2r_rand_state()) == 4);
    pArea->SortLists();
    ClearMessages();
}

// The door (CProjectile84E940, DDoorH, 0x535320): fired at a sprite
// (0x535D20) it stands one above it and delivers its effects; it plays
// "DDoorH2", "DDoorH1" four times and "DDoorH2" backwards (0x5357B0), then
// arrives. No rand draws.
TEST_CASE("visual projectiles: the door opens, loops, closes and arrives", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();
    const CPoint pos = pSprite->GetPos();

    CProjectile84E940* pDoor = new CProjectile84E940(CString("DDoorH"), CString(""), CString(""), 0, 0);
    CHECK(pDoor->m_sAnimation == "DDoorH");
    CHECK(pDoor->m_nState == 0);
    CHECK(pDoor->m_bPalette == FALSE);
    CHECK(pDoor->m_animation.GetResRef() == CResRef("DDoorH2"));

    iwd2r_srand(51);
    pDoor->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pos, 0, 0);
    const LONG id = pDoor->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pDoor->GetPos() == CPoint(pos.x, pos.y - 1));
    CHECK(AddedOne<CProjectile84E940>(pArea) == pDoor);
    ClearMessages();

    {
        AreaScope scope(pArea);
        REQUIRE(scope.Ok());
        scope.CentreOn(pos);
        CPoint ptReference;
        CSize frameSize;
        pDoor->m_animation.GetCurrentCenterPoint(ptReference, FALSE);
        pDoor->m_animation.GetCurrentFrameSize(frameSize, FALSE);
        const CPoint ptPos(pos.x, pos.y - 1 + pArea->GetHeightOffset(pDoor->GetPos(), pDoor->m_listType));
        const CRect rBounds(ptPos.x - ptReference.x, ptPos.y - ptReference.y, ptPos.x - ptReference.x + frameSize.cx, ptPos.y - ptReference.y + frameSize.cy);
        const Lit lit = RenderLit(scope, pDoor, rBounds, "door");
        CHECK(lit.inside > 0);
        CHECK(lit.outside == 0);
    }

    std::vector<int> states;
    for (int frame = 0; frame < 2000 && Alive(id); frame++) {
        if (states.empty() || states.back() != pDoor->m_nState) {
            states.push_back(pDoor->m_nState);
        }
        if (pDoor->m_nState == 1 && states.size() == 2) {
            CHECK(pDoor->m_animation.GetResRef() == CResRef("DDoorH1"));
        }
        pDoor->AIUpdate();
    }
    CHECK(!Alive(id));
    CHECK(states == std::vector<int> { 0, 1, 2, 3, 4, 5 });
    CHECK(Draws(51, iwd2r_rand_state()) == 0);
    pArea->SortLists();
    ClearMessages();
}

// The Visual Spell Hit effect (opcode 141, CGameEffectVisualSpellHit::
// ApplyEffect 0x4BBCB0) as spells carry it, applied to a goblin in AR1000:
// SPWI984's three hits (modes 24, 25, 26: air, earth and water in palette
// 6) queue the projectile of their type (0x88..0x8A) and play their local
// hit (CProjectile84E9C0, 0x535F20) at the goblin until it arrives;
// SPWI980's burst (mode 32) and SPWI995's door (mode 38) play their
// objects. The hits are drawn into the shim surfaces on the way.
TEST_CASE("visual projectiles: a spell's Visual Spell Hit plays on a goblin and finishes", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x + 70, world.pCharacter->GetPos().y + 20);
    CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
    REQUIRE(pGoblin != NULL);
    pArea->SortLists();
    ClearMessages();

    // The opcode 141 effects of the spell's first ability.
    auto hits = [&](const char* resRef) {
        std::vector<std::unique_ptr<CGameEffect>> effects;
        CSpell spell { CResRef(resRef) };
        for (LONG n = 0; n < 32; n++) {
            std::unique_ptr<CGameEffect> pEffect(spell.GetAbilityEffect(0, n, pGoblin, 0, 0, 0));
            if (pEffect == nullptr) {
                break;
            }
            if (pEffect->m_effectID == CGAMEEFFECT_VISUALSPELLHIT) {
                effects.push_back(std::move(pEffect));
            }
        }
        return effects;
    };

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(pos);

    {
        INFO("SPWI984");
        std::vector<std::unique_ptr<CGameEffect>> effects = hits("SPWI984");
        REQUIRE(effects.size() == 3);
        const DWORD modes[] = { 24, 25, 26 };
        const WORD types[] = { 0x88, 0x89, 0x8A };
        const char* animations[] = { "SHAir", "SHEarth", "SHWater" };
        const char* palettes[] = { "SHAir6", "SHEarth6", "SHWater6" };
        for (size_t n = 0; n < effects.size(); n++) {
            INFO("mode " << modes[n]);
            CGameEffect* pEffect = effects[n].get();
            CHECK(pEffect->m_dwFlags == modes[n]);
            CHECK(pEffect->ApplyEffect(pGoblin) == TRUE);
            CHECK(pEffect->m_done == TRUE);

            CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
            REQUIRE(handler.m_messageList.GetCount() == 2);
            CMessageFireProjectile* message = dynamic_cast<CMessageFireProjectile*>(handler.m_messageList.GetHead());
            REQUIRE(message != nullptr);
            CHECK(message->m_projectileType == types[n]);
            CHECK(message->m_projectileTargetId == pGoblin->GetId());
            CHECK(message->GetSource() == pGoblin->GetId());
            CHECK(dynamic_cast<CMessageGroup*>(handler.m_messageList.GetTail()) != nullptr);
            ClearMessages();

            CProjectile84E9C0* pHit = AddedOne<CProjectile84E9C0>(pArea);
            REQUIRE(pHit != nullptr);
            CHECK(pHit->m_animation.GetResRef() == CResRef(animations[n]));
            CHECK(pHit->m_bPalette == TRUE);
            CHECK(pHit->m_palette.GetResRef() == CResRef(palettes[n]));
            CHECK(pHit->m_dwFlags == 8);
            CHECK(pHit->m_bHasHeight == TRUE);
            CHECK(pHit->GetPos() == CPoint(pos.x, pos.y + 1));
            CHECK(pHit->m_posZ == pHit->DetermineHeight(pGoblin));
            CHECK(pHit->m_posZ == pGoblin->GetAnimation()->GetCastHeight());

            const std::vector<DWORD> pixels = RenderObject(scope, pHit);
            scope.Shot(pixels, std::string("hit_") + animations[n]);
            const Lit lit = CountLit(pixels, scope.Screen(), CRect());
            CHECK(lit.outside > 0);
            CHECK(pHit->m_dwFlags == (8 | CInfinity::FXPREP_COPYFROMBACK));

            const LONG id = pHit->GetId();
            pArea->SortLists();
            iwd2r_srand(61);
            CHECK(UpdateUntilGone(id, 400) > 0);
            CHECK(Draws(61, iwd2r_rand_state()) == 0);
            pArea->SortLists();
            ClearMessages();
        }
    }

    {
        INFO("SPWI980");
        std::vector<std::unique_ptr<CGameEffect>> effects = hits("SPWI980");
        REQUIRE(effects.size() == 1);
        CHECK(effects[0]->m_dwFlags == 32);
        effects[0]->ApplyEffect(pGoblin);
        CHECK(effects[0]->m_done == TRUE);
        CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());
        CProjectile84DFA4* pBurst = AddedOne<CProjectile84DFA4>(pArea);
        REQUIRE(pBurst != nullptr);
        CHECK(pBurst->GetPos() == CPoint(pos.x, pos.y - 2));
        // Drawn on the goblin's square: projectiles take the visible
        // terrain table (0x8A8154), whose entry for a square with a creature
        // on it (8) is passable.
        const Lit lit = RenderLit(scope, pBurst, CRect(), "spwi980");
        CHECK(lit.outside > 0);
        const LONG id = pBurst->GetId();
        pArea->SortLists();
        CHECK(UpdateUntilGone(id, 400) > 0);
        pArea->SortLists();
    }

    {
        INFO("SPWI995");
        std::vector<std::unique_ptr<CGameEffect>> effects = hits("SPWI995");
        REQUIRE(effects.size() == 1);
        CHECK(effects[0]->m_dwFlags == 38);
        effects[0]->ApplyEffect(pGoblin);
        CHECK(effects[0]->m_done == TRUE);
        CProjectile84E940* pDoor = AddedOne<CProjectile84E940>(pArea);
        REQUIRE(pDoor != nullptr);
        CHECK(pDoor->m_sAnimation == "DDoorH");
        CHECK(pDoor->GetPos() == CPoint(pos.x, pos.y - 1));
        ClearMessages();
        const LONG id = pDoor->GetId();
        pArea->SortLists();
        CHECK(UpdateUntilGone(id, 2000) > 0);
        pArea->SortLists();
        ClearMessages();
    }
}

// A sparkling projectile in flight (CProjectileBAM::AIUpdate 0x52B900 at
// 0x52BC88, CProjectileSingleTarget::AIUpdate 0x578AB0 at 0x578E38) leaves
// a puff of its sparkle colour (TYPE_SPARKLE, 14 particles, 28 draws) at
// its position and height each update.
TEST_CASE("visual projectiles: a sparkling projectile leaves a puff each update", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    const CPoint pos(pSprite->GetPos().x + 200, pSprite->GetPos().y + 60);
    CGameSprite* pGoblin = AddCreature(pArea, "00GOB", pos);
    REQUIRE(pGoblin != NULL);
    pArea->SortLists();
    ClearMessages();

    CProjectile* projectiles[] = {
        new CProjectile84DF1C(),
        new CProjectileSingleTarget(CResRef("SPMAGMIS")),
    };
    for (CProjectile* pProjectile : projectiles) {
        pProjectile->m_bSparkle = TRUE;
        pProjectile->m_nSparkleColour = 8;
        pProjectile->Fire(pArea, pSprite->GetId(), pGoblin->GetId(), pos, 0, 0);
        const LONG id = pProjectile->GetId();
        REQUIRE(id != CGameObjectArray::INVALID_INDEX);
        pArea->SortLists();

        iwd2r_srand(71);
        pProjectile->AIUpdate();
        CHECK(Draws(71, iwd2r_rand_state()) == 28);
        std::vector<CGameObject84D624*> added = AddedParticles(pArea);
        REQUIRE(added.size() == 1);
        CHECK(added[0]->m_wType == CGameObject84D624::TYPE_SPARKLE);
        // CSPARKLE_COLORTYPE_WHITE.
        CHECK(added[0]->m_rgbColors[0] == RGB(245, 245, 245));
        CHECK(added[0]->GetPos() == pProjectile->GetPos());
        CHECK(added[0]->m_posZ == pProjectile->m_posZ);
        CHECK(added[0]->GetVertListType() == CGameObject::LIST_FRONT);
        CHECK(added[0]->m_particleList.GetCount() == 14);

        pProjectile->RemoveSelf();
        CHECK(UpdateUntilGone(added[0]->GetId(), 400) > 0);
        pArea->SortLists();
        ClearMessages();
    }
}

// The explosion's streams for other directions (0x51CE50): a source due
// north fans them north-west, north and north-east; a source south-west of
// the position (as far south as west) south, south-west and west.
TEST_CASE("visual projectiles: an explosion fans its streams away from its source", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    const CPoint pos(world.pCharacter->GetPos().x - 70, world.pCharacter->GetPos().y - 30);

    struct Fan {
        CPoint source;
        CPoint vel[3];
    };
    const Fan fans[] = {
        { CPoint(pos.x, pos.y - 100), { CPoint(-700, -700), CPoint(0, -1000), CPoint(700, -700) } },
        { CPoint(pos.x - 100, pos.y + 100), { CPoint(0, 1000), CPoint(-700, 700), CPoint(-1000, 0) } },
    };
    for (const Fan& fan : fans) {
        INFO("source " << fan.source.x - pos.x << "," << fan.source.y - pos.y);
        CGameObject84D624* pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_STONE,
            CGameObject84D624::TYPE_EXPLOSION,
            CGameObject84D624::EXPLOSION_PUFFS);
        pExplosion->SetSource(fan.source);
        pExplosion->AddToArea(pArea, pos, 30, CGameObject::LIST_FRONT);
        REQUIRE(pExplosion->m_particleList.GetCount() == 42);
        for (int stream = 0; stream < 3; stream++) {
            CParticle* pFirst = pExplosion->m_particleList.GetAt(pExplosion->m_particleList.FindIndex(14 * stream));
            CHECK(pFirst->m_vel.x == fan.vel[stream].x);
            CHECK(pFirst->m_vel.y == fan.vel[stream].y);
        }
        CHECK(UpdateUntilGone(pExplosion->GetId(), 400) > 0);
        pArea->SortLists();
    }
}

// CProjectileSingleTarget::Render (0x578480) draws a spell hit
// (CProjectileSpellHit, DecodeSpellHitProjectile type 9) on the creature it
// was fired at: on its square, which the projectiles' visible terrain
// table passes.
TEST_CASE("visual projectiles: a spell hit draws on its target", "[visualprojectile][data]")
{
    WORLD_OR_SKIP(world);
    CGameArea* pArea = world.pArea;
    CGameSprite* pSprite = world.pCharacter;
    pArea->SortLists();
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(9, NULL, FALSE);
    REQUIRE(pProjectile != nullptr);
    pProjectile->Fire(pArea, pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);
    const LONG id = pProjectile->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    ClearMessages();

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(pSprite->GetPos());
    // A few frames in, its animation is under way.
    pProjectile->AIUpdate();
    pProjectile->AIUpdate();
    const CPoint pos = pSprite->GetPos();
    const Lit lit = RenderLit(scope, pProjectile, CRect(pos.x - 200, pos.y - 250, pos.x + 200, pos.y + 100), "spellhit");
    CHECK(lit.inside > 0);
    CHECK(lit.outside == 0);

    CHECK(UpdateUntilGone(id, 2000) > 0);
    pArea->SortLists();
    ClearMessages();
}
