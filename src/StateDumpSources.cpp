// The engine's side of the replay state dump (platform/state_dump.h): reads
// the fields of oracle/replay/schema.json from our classes, the same members
// the replay recorder reads from the original through fields.json (its
// x-fields name them), and registers itself at start-up. Only the replay
// mode (platform/replay.h, IWD2R_STATE_DUMP) calls it.

#include "mfc.h"

#include "CAIScript.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameDialog.h"
#include "CGameDoor.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CGameTimer.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMessage.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CTimerWorld.h"
#include "platform/replay.h"
#include "platform/state_dump.h"

#include <cstring>

namespace {

using platform::state_dump::Chargen;
using platform::state_dump::Dialog;
using platform::state_dump::Door;
using platform::state_dump::EngineState;
using platform::state_dump::Game;
using platform::state_dump::GameMember;
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

// The object an id names (the create_char screen's sprite, a party member,
// an object on an area's list): CGameObjectArray::GetShare's THREAD_ASYNCH
// lookup without the call (no share taken), the recorder's ObjectById.
// nullptr when the id does not resolve.
CGameObject* ObjectById(CBaldurChitin* chitin, LONG id)
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
    return entry.m_objectPtr;
}

// A sprite's id, the recorder's SpriteById: the object, unchecked.
CGameSprite* SpriteById(CBaldurChitin* chitin, LONG id)
{
    return static_cast<CGameSprite*>(ObjectById(chitin, id));
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

// The objects of type on an area list, head to tail (ids, as the lists
// store them), the recorder's ObjectsOn.
template <typename Visit>
void ObjectsOn(CBaldurChitin* chitin, CTypedPtrList<CPtrList, int*>& list, BYTE type, Visit visit)
{
    POSITION pos = list.GetHeadPosition();
    while (pos != NULL) {
        const LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list.GetNext(pos)));
        CGameObject* object = ObjectById(chitin, id);
        if (object && object->m_objectType == type)
            visit(object);
    }
}

// The area's doors, explored tiles and sprites, the recorder's ReadArea.
void ReadArea(CBaldurChitin* chitin, CGameArea* area, World* out)
{
    ObjectsOn(chitin, area->m_lVertSort, CGameObject::TYPE_DOOR, [&](CGameObject* object) {
        const CGameDoor* door = static_cast<CGameDoor*>(object);
        Door d;
        d.name.assign(door->m_scriptName, strnlen(door->m_scriptName, SCRIPTNAME_SIZE));
        d.open = (door->m_dwFlags & 0x1) != 0;
        d.locked = (door->m_dwFlags & 0x2) != 0;
        out->doors.push_back(d);
    });
    const CVisibilityMap& visibility = area->m_visibility;
    if (visibility.m_pMap) {
        for (LONG i = 0; i < visibility.m_nMapSize; ++i) {
            if ((visibility.m_pMap[i] & CVisibilityMap::EXPLORED_FLAG) != 0)
                ++out->exploredCount;
        }
    }
    for (CTypedPtrList<CPtrList, int*>* list : { &area->m_lVertSort, &area->m_lVertSortBack, &area->m_lVertSortFlight })
        ObjectsOn(chitin, *list, CGameObject::TYPE_SPRITE, [&](CGameObject*) { ++out->creatures; });
}

// The world screen's dialog, the recorder's DialogJson.
void ReadDialog(CGameDialogSprite& dialog, Dialog* out)
{
    out->active = dialog.field_54 != 0;
    if (!out->active)
        return;
    out->speaker = dialog.m_talkerIndex;
    out->entry = dialog.m_currentEntryIndex;
    if (!dialog.m_waitingForResponse || dialog.m_currentEntryIndex >= static_cast<DWORD>(dialog.m_dialogEntries.GetSize()))
        return;
    CGameDialogEntry* entry = dialog.m_dialogEntries[dialog.m_currentEntryIndex];
    if (!entry)
        return;
    for (INT i = 0; i < entry->GetSize(); ++i) {
        const CGameDialogReply* reply = entry->GetAt(i);
        if (reply && reply->m_displayPosition != NULL)
            ++out->responses;
    }
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
            member.action = sprite->m_curAction.m_actionID;
            member.modal = sprite->m_nModalState;
            member.facing = sprite->m_nDirection;
        }
        out->party.push_back(member);
    }
    if (game->m_visibleArea < CINFGAME_MAX_AREAS && game->m_gameAreas[game->m_visibleArea]) {
        // The AI thread re-sorts the area's lists (CGameArea::SortLists);
        // they are walked under the list lock, as CGameArea::Render walks
        // them. The replay mode reads while the AI thread waits, so the
        // lock is free there; a real-input run's state request
        // (platform::script::PollControl) reads while it runs.
        CGameArea* area = game->m_gameAreas[game->m_visibleArea];
        game->BeginListManipulation(area);
        ReadArea(chitin, area, out);
        game->EndListManipulation(area);
    }
    if (chitin->m_pEngineWorld)
        ReadDialog(chitin->m_pEngineWorld->m_internalLoadedDialog, &out->dialog);
}

// The party's purse and what a save carries of each member, the recorder's
// GameJson. NOTE: A real-input run's state request reads the item pointers
// while the AI thread runs; the replay mode reads while it waits.
void ReadGame(CBaldurChitin* chitin, CInfGame* game, Game* out)
{
    out->partyGold = game->m_gameSave.m_nPartyGold;
    SHORT count = game->m_nCharacters;
    if (count < 0)
        count = 0;
    if (count > 6)
        count = 6;
    for (SHORT i = 0; i < count; ++i) {
        GameMember member;
        member.id = game->m_characterPortraits[i];
        CGameSprite* sprite = SpriteById(chitin, member.id);
        member.sprite = sprite != nullptr;
        if (sprite) {
            const CCreatureFileHeader& stats = sprite->m_baseStats;
            member.gold = stats.m_gold;
            member.xp = stats.m_xp;
            member.level = stats.m_characterLevel;
            for (int k = 0; k < 11; ++k)
                member.classLevels[k] = stats.m_classLevels[k];
            for (int slot = 0; slot < 51; ++slot) {
                const CItem* item = sprite->m_equipment.m_items[slot];
                member.hasItem[slot] = item != nullptr;
                if (!item)
                    continue;
                char resref[RESREF_SIZE + 1] = {};
                std::memcpy(resref, item->cResRef.GetResRef(), RESREF_SIZE);
                member.items[slot].resref = resref;
                member.items[slot].uses[0] = item->m_useCount1;
                member.items[slot].uses[1] = item->m_useCount2;
                member.items[slot].uses[2] = item->m_useCount3;
                member.items[slot].flags = item->m_flags;
            }
        }
        out->members.push_back(member);
    }
}

// IWD2R_PROBE's block (platform/replay_probe.h), the recorder's ReadProbe.
void ReadProbe(platform::replay_probe::Frame* out)
{
    CBaldurChitin* chitin = g_pBaldurChitin;
    if (!chitin) {
        out->screen = "none";
        return;
    }
    out->screen = ScreenName(chitin);
    // NOTE: The AI thread runs and fills the queue; the frame gate holds it
    // at the frame's start. Diagnostic runs only.
    const CTypedPtrList<CPtrList, CMessage*>& messages = chitin->m_cMessageHandler.m_messageList;
    out->messages = static_cast<int>(messages.GetCount());
    POSITION messagePos = messages.GetHeadPosition();
    while (messagePos != NULL) {
        CMessage* message = messages.GetNext(messagePos);
        if (CMessageSetSequence* setSequence = dynamic_cast<CMessageSetSequence*>(message)) {
            platform::replay_probe::Frame::SetSequence m;
            m.target = setSequence->m_targetId;
            m.sequence = setSequence->m_sequence;
            m.source = setSequence->m_sourceId;
            out->setSequences.push_back(m);
        }
    }
    CInfGame* game = chitin->m_pObjectGame;
    if (!game)
        return;
    out->characters = game->m_nCharacters;
    out->visibleArea = game->m_visibleArea;
    out->gameTime = game->m_worldTime.m_gameTime;
    out->timeActive = game->m_worldTime.m_active;
    for (int i = 0; i < 6; ++i) {
        out->slots[i] = game->m_characters[i];
        out->portraits[i] = game->m_characterPortraits[i];
    }
    CGameArea* area = game->m_visibleArea < CINFGAME_MAX_AREAS ? game->m_gameAreas[game->m_visibleArea] : nullptr;
    if (area) {
        platform::replay_probe::Area& a = out->area;
        a.present = true;
        char resref[RESREF_SIZE + 1] = {};
        std::memcpy(resref, area->m_resRef.GetResRef(), RESREF_SIZE);
        a.resref = resref;
        a.characters = area->m_nCharacters;
        a.viewX = area->m_cInfinity.nCurrentX;
        a.viewY = area->m_cInfinity.nCurrentY;
        const CVisibilityMap& visibility = area->m_visibility;
        a.width = visibility.m_nWidth;
        a.height = visibility.m_nHeight;
        a.size = visibility.m_nMapSize;
        a.range = visibility.field_E;
        for (int i = 0; i < 6; ++i)
            a.ids[i] = visibility.m_aCharacterIds[i];
        if (visibility.m_pMap && visibility.m_nMapSize > 0)
            a.map.assign(visibility.m_pMap, visibility.m_pMap + visibility.m_nMapSize);
    }
    const CGameObjectArray& objects = game->m_cObjectArray;
    for (SHORT index = 0; objects.m_pArray && index <= objects.m_nMaxArrayIndex; ++index) {
        const CGameObjectArrayEntry& entry = objects.m_pArray[index];
        CGameObject* object = entry.m_objectPtr;
        if (entry.m_objectId == -1 || !object)
            continue;
        platform::replay_probe::Object o;
        o.index = index;
        o.id = (static_cast<LONG>(index) << 16) | static_cast<WORD>(entry.m_objectId);
        o.type = object->m_objectType;
        o.x = object->m_pos.x;
        o.y = object->m_pos.y;
        o.here = area && object->m_pArea == area;
        o.list = object->m_listType;
        o.seen = object->m_canBeSeen;
        o.speed = object->m_AISpeed;
        o.inhibitor = object->m_AIInhibitor;
        if (o.type == CGameObject::TYPE_SPRITE) {
            const CGameSprite* sprite = static_cast<CGameSprite*>(object);
            o.hasName = true;
            o.name.assign(sprite->m_scriptName, strnlen(sprite->m_scriptName, SCRIPTNAME_SIZE));
            o.sprite = true;
            o.lastCharacters = sprite->m_lastCharacterCount;
            o.active = sprite->m_active;
            o.activeAi = sprite->m_activeAI;
            o.activeImprisonment = sprite->m_activeImprisonment;
            o.visibleMonster = sprite->m_bVisibleMonster;
            o.field294 = sprite->m_baseStats.field_294;
            o.generalState = sprite->m_derivedStats.m_generalState;
            o.action = sprite->m_curAction.m_actionID;
            o.sequence = sprite->m_nSequence;
            o.queued = sprite->m_queuedActions.GetCount();
            if (!sprite->m_queuedActions.IsEmpty())
                o.queuedHead = sprite->m_queuedActions.GetHead()->m_actionID;
            o.triggers = sprite->m_pendingTriggers.GetCount();
            o.timedEffects = sprite->m_timedEffectList.GetCount();
            o.equipedEffects = sprite->m_equipedEffectList.GetCount();
            o.xp = sprite->m_baseStats.m_xp;
            o.hp = sprite->m_baseStats.m_hitPoints;
            // NOTE: The AI thread owns m_pPath and m_timers; with IWD2R_PROBE
            // set these reads race it (the frame gate keeps it waiting at a
            // frame's start, but a path the search thread hands over is not
            // locked here). Diagnostic runs only.
            o.destX = sprite->m_posDest.x;
            o.destY = sprite->m_posDest.y;
            for (SHORT n = 0; sprite->m_pPath != NULL && n < sprite->m_nPath && n < 64; n++)
                o.path.push_back(sprite->m_pPath[n]);
            POSITION pos = sprite->m_timers.GetHeadPosition();
            while (pos != NULL) {
                const CGameTimer* timer = sprite->m_timers.GetNext(pos);
                o.timers.emplace_back(timer->m_id, timer->m_time);
            }
            CAIScript* const scripts[] = {
                sprite->m_overrideScript, sprite->m_special1Script, sprite->m_teamScript,
                sprite->m_special2Script, sprite->m_combatScript, sprite->m_special3Script,
                sprite->m_movementScript,
            };
            for (CAIScript* script : scripts) {
                if (script == NULL) {
                    o.scripts.emplace_back("-", 0);
                    continue;
                }
                char resref[RESREF_SIZE + 1] = {};
                std::memcpy(resref, script->m_cResRef.GetResRef(), RESREF_SIZE);
                o.scripts.emplace_back(resref, static_cast<int>(script->m_caList.GetCount()));
            }
            o.direction = sprite->m_nDirection;
            o.newDirection = sprite->m_nNewDirection;
            o.noActionCount = sprite->m_noActionCount;
        } else if (o.type == CGameObject::TYPE_DOOR) {
            const CGameDoor* door = static_cast<CGameDoor*>(object);
            o.hasName = true;
            o.name.assign(door->m_scriptName, strnlen(door->m_scriptName, SCRIPTNAME_SIZE));
        }
        out->objects.push_back(o);
    }
}

const bool g_probeRegistered = (platform::replay::SetProbeSource(&ReadProbe), true);

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
        out->hasGame = chitin->m_pObjectGame != nullptr;
        if (out->hasGame)
            ReadGame(chitin, chitin->m_pObjectGame, &out->game);
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
