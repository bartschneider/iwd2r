// The engine's side of the replay state dump (platform/state_dump.h): reads
// the fields of oracle/replay/schema.json from our classes, the same members
// the replay recorder reads from the original through fields.json (its
// x-fields name them), and registers itself at start-up. Only the replay
// mode (platform/replay.h, IWD2R_STATE_DUMP) calls it.

#include "mfc.h"

#include "CBaldurChitin.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CScreenCreateChar.h"
#include "platform/state_dump.h"

#include <cstring>

namespace {

using platform::state_dump::Chargen;
using platform::state_dump::EngineState;

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

// The object the create_char screen edits: CGameObjectArray::GetShare's
// THREAD_ASYNCH lookup without the call (no share taken), the recorder's
// ChargenSprite. nullptr when the id does not resolve.
CGameSprite* ChargenSprite(CBaldurChitin* chitin, LONG id)
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
    CGameSprite* sprite = ChargenSprite(chitin, screen->m_nGameSprite);
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
        out->hasChargen = out->screen == "create_char";
        if (out->hasChargen)
            ReadChargen(chitin, chitin->m_pEngineCreateChar, &out->chargen);
    }
};

const EngineDumpSource g_source;

}
