// The engine's side of the replay state dump (platform/state_dump.h): reads
// the fields of oracle/replay/schema.json from our classes, the same members
// the replay recorder reads from the original through fields.json (its
// x-fields name them), and registers itself at start-up. Only the replay
// mode (platform/replay.h, IWD2R_STATE_DUMP) calls it.

#include "mfc.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CScreenCreateChar.h"
#include "platform/state_dump.h"

#include <cstring>

namespace {

using platform::state_dump::Chargen;
using platform::state_dump::EngineState;
using platform::state_dump::PartyMember;
using platform::state_dump::World;

// The active engine named by the m_pEngine* member it equals, by the
// recorder's names (fields.json engine_*) and in its order (the last match
// wins, as there); none or unknown. Compared as addresses, as the recorder
// compares the raw pointers.
const char* ScreenName(CBaldurChitin* chitin)
{
    const void* active = chitin->pActiveEngine;
    if (!active)
        return "none";
    const struct {
        const char* name;
        const void* engine;
    } screens[] = {
        { "dm", chitin->m_pEngineDM },
        { "projector", chitin->m_pEngineProjector },
        { "character", chitin->m_pEngineCharacter },
        { "create_char", chitin->m_pEngineCreateChar },
        { "inventory", chitin->m_pEngineInventory },
        { "journal", chitin->m_pEngineJournal },
        { "load", chitin->m_pEngineLoad },
        { "map", chitin->m_pEngineMap },
        { "options", chitin->m_pEngineOptions },
        { "save", chitin->m_pEngineSave },
        { "spellbook", chitin->m_pEngineSpellbook },
        { "start", chitin->m_pEngineStart },
        { "world", chitin->m_pEngineWorld },
        { "store", chitin->m_pEngineStore },
        { "single_player", chitin->m_pEngineSinglePlayer },
        { "connection", chitin->m_pEngineConnection },
        { "world_map", chitin->m_pEngineWorldMap },
        { "chapter", chitin->m_pEngineChapter },
        { "movies", chitin->m_pEngineMovies },
        { "keymaps", chitin->m_pEngineKeymaps },
    };
    const char* name = "unknown";
    for (const auto& screen : screens) {
        if (screen.engine == active)
            name = screen.name;
    }
    return name;
}

// The sprite an object id names (the create_char screen's, a party
// member's): CGameObjectArray::GetShare's THREAD_ASYNCH lookup without the
// call (no share taken), the recorder's SpriteById. nullptr when the id does
// not resolve.
CGameSprite* SpriteById(CBaldurChitin* chitin, LONG id)
{
    CInfGame* game = chitin->m_pObjectGame;
    if (!game)
        return nullptr;
    const CGameObjectArray& objects = game->m_cObjectArray;
    const SHORT index = static_cast<SHORT>((id >> 16) & 0x7FFF);
    if (id == -1 || objects.m_nMaxArrayIndex < index || (id & 0x8000) != 0
        || objects.m_nNextObjectId <= static_cast<SHORT>(id) || !objects.m_pArray) {
        return nullptr;
    }
    const CGameObjectArrayEntry& entry = objects.m_pArray[index];
    if (entry.m_objectId != static_cast<SHORT>(id))
        return nullptr;
    return static_cast<CGameSprite*>(entry.m_objectPtr);
}

void ReadChargen(CBaldurChitin* chitin, CScreenCreateChar* screen, Chargen* out)
{
    const BYTE memory[] = { screen->m_nMemorySTR, screen->m_nMemoryDEX, screen->m_nMemoryCON,
        screen->m_nMemoryINT, screen->m_nMemoryWIS, screen->m_nMemoryCHR };
    for (int i = 0; i < 6; ++i)
        out->rolls[i] = memory[i];
    out->rolls[6] = screen->m_nMemoryExtra;
    out->skillPoints = screen->m_nExtraSkillPoints;
    CGameSprite* sprite = SpriteById(chitin, screen->m_nGameSprite);
    out->sprite = sprite != nullptr;
    if (!sprite)
        return;
    out->race = sprite->m_startTypeAI.m_nRace;
    out->subrace = sprite->m_startTypeAI.m_nSubRace;
    out->klass = sprite->m_startTypeAI.m_nClass;
    const CCreatureFileHeader& stats = sprite->m_baseStats;
    const BYTE abilities[] = { stats.m_STRBase, stats.m_DEXBase, stats.m_CONBase, stats.m_INTBase, stats.m_WISBase,
        stats.m_CHRBase };
    for (int i = 0; i < 6; ++i)
        out->abilities[i] = abilities[i];
    for (int i = 0; i < 3; ++i)
        out->feats[i] = stats.m_feats[i];
    char resref[sizeof(stats.m_portraitLarge) + 1] = {};
    std::memcpy(resref, &stats.m_portraitLarge, sizeof(stats.m_portraitLarge));
    out->portrait = resref;
}

// The area on view and the party, the recorder's WorldJson: GetVisibleArea
// and GetCharacterId read without the calls.
void ReadWorld(CBaldurChitin* chitin, World* out)
{
    CInfGame* game = chitin->m_pObjectGame;
    if (!game)
        return;
    if (game->m_visibleArea < CINFGAME_MAX_AREAS) {
        CGameArea* area = game->m_gameAreas[game->m_visibleArea];
        if (area) {
            char resref[RESREF_SIZE + 1] = {};
            std::memcpy(resref, area->m_resRef.GetResRef(), RESREF_SIZE);
            out->area = resref;
        }
    }
    SHORT count = game->m_nCharacters;
    if (count < 0)
        count = 0;
    if (count > 6)
        count = 6;
    for (SHORT i = 0; i < count; ++i) {
        PartyMember member;
        member.id = game->m_characterPortraits[i];
        CGameSprite* sprite = SpriteById(chitin, member.id);
        member.sprite = sprite != nullptr;
        if (sprite) {
            member.x = sprite->m_pos.x;
            member.y = sprite->m_pos.y;
            member.hp = sprite->m_baseStats.m_hitPoints;
            member.maxHp = sprite->m_derivedStats.m_nMaxHitPoints;
            for (int k = 0; k < 64; ++k)
                member.skills[k] = sprite->m_derivedStats.m_nSkills[k];
        }
        out->party.push_back(member);
    }
}

class EngineDumpSource : public platform::state_dump::DumpSource {
public:
    EngineDumpSource() { platform::state_dump::SetSource(this); }

    bool Ready() const override { return g_pBaldurChitin != nullptr; }

    void Read(EngineState* out) const override
    {
        CBaldurChitin* chitin = g_pBaldurChitin;
        if (!chitin)
            return;
        out->screen = ScreenName(chitin);
        out->auCounter = chitin->nAUCounter;
        out->fullscreen = chitin->m_bFullscreen;
        out->inAsyncUpdate = chitin->m_bInAsynchronousUpdate;
        out->reinitializing = chitin->m_bReInitializing;
        out->loading = chitin->cProgressBar.m_bProgressBarActivated;
        out->hasChargen = out->screen == "create_char";
        if (out->hasChargen)
            ReadChargen(chitin, chitin->m_pEngineCreateChar, &out->chargen);
        out->hasWorld = out->screen == "world";
        if (out->hasWorld)
            ReadWorld(chitin, &out->world);
    }

    void ReadAi(platform::state_dump::AiState* out) const override
    {
        CBaldurChitin* chitin = g_pBaldurChitin;
        if (!chitin)
            return;
        // The AI thread writes these. volatile only keeps the compiler from
        // merging or reordering the reads; the order the hardware sees is
        // x86's, where loads are not reordered with loads and the writer's
        // store order suffices. An ARM build needs acquire loads here and
        // release stores on the AI thread (std::atomic_ref) or a fence:
        // otherwise the gate could read in_async_update before au_counter,
        // hold the update's last redraw, and the run would hang until its
        // timeout.
        out->auCounter = *static_cast<volatile LONG*>(&chitin->nAUCounter);
        out->inAsyncUpdate = *static_cast<volatile BOOL*>(&chitin->m_bInAsynchronousUpdate);
        out->loading = *static_cast<volatile BOOLEAN*>(&chitin->cProgressBar.m_bProgressBarActivated);
    }
};

const EngineDumpSource g_source;

}
