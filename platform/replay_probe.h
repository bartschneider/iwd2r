// The replay probe (IWD2R_PROBE, docs/rand-trace.md): a text record of the
// state a divergence hunt needs and the dumps leave out, written by both
// engines the same way so that `diff original/probe.txt ours/probe.txt`
// names the first difference. Shared by the recorder
// (oracle/replay/recorder.cpp, reading the original through fields.json)
// and our replay mode (platform/replay.cpp, filled by
// src/StateDumpSources.cpp): each side fills a Frame, this header formats
// it, so the two texts cannot drift apart in format.
//
// IWD2R_PROBE=FIRST-LAST (or one frame number) turns it on for those
// frames. At each, OnFrame (the frame's start, before its AI update, as the
// dumps) appends one block to probe.txt:
//
//   frame N screen NAME
//   game characters C visible_area V slots a,b,c,d,e,f portraits a,b,c,d,e,f
//       time T active A
//   area RESREF characters C view X,Y vis WxH size S range R ids a,b,c,d,e,f
//   obj INDEX ID type T pos X,Y here H list L seen S speed P inhibitor I
//       [name NAME] [last C active A ai B imprisonment M visible_monster V
//       field_294 F state S action A sequence Q queued N next A triggers T
//       effects TIMED,EQUIPED xp X hp H dest X,Y path [NODE ...]
//       timers [ID:TIME ...]]
//   vis ROW HEX...
//
// - game: CInfGame::m_nCharacters, m_visibleArea, m_characters (the
//   party slots Player1..6 resolve through), m_characterPortraits, and
//   the world timer's m_gameTime and m_active.
// - area: the area on view (m_gameAreas[m_visibleArea]): m_resRef,
//   m_nCharacters, the viewport's m_cInfinity.nCurrentX/Y, and its
//   CVisibilityMap's m_nWidth, m_nHeight, m_nMapSize, field_E (the
//   visibility range in squares) and m_aCharacterIds. Absent with no area.
// - obj: every live entry of the object array (CInfGame::m_cObjectArray,
//   index 0 .. m_nMaxArrayIndex), in index order: its id, the CGameObject
//   members m_objectType, m_pos, whether m_pArea is the area on view
//   (here 1/0), m_listType, m_canBeSeen, m_AISpeed and m_AIInhibitor; a
//   sprite (0x31) or door (0x21) adds its script name; a sprite adds
//   m_lastCharacterCount, m_active, m_activeAI, m_activeImprisonment,
//   m_bVisibleMonster, m_baseStats.field_294, m_derivedStats.m_generalState
//   (hex), m_curAction.m_actionID, m_nSequence, the action queue's
//   length and its head's m_actionID (-1 when empty), and the pending
//   triggers' count, the timed and equipped effect lists' lengths, the
//   experience (m_baseStats.m_xp), the hit points and the script timers
//   (m_timers: each CGameTimer's id and expiry time), m_posDest and the
//   path's nodes (m_pPath[0 .. m_nPath - 1], at most 64). One line per object.
// - vis: the area's visibility map, one line per row of m_nWidth tiles
//   (bytes in hex: bit 7 explored, the low bits the visibility count);
//   the rows past m_nMapSize, if any, are left out.
//
// The original's last frame before it exits is written, unlike the rand
// trace: the probe is written whole at the frame's start.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace platform::replay_probe {

// IWD2R_PROBE's frames.
struct Range {
    bool on = false;
    unsigned first = 0;
    unsigned last = 0;

    bool Has(unsigned frame) const { return on && first <= frame && frame <= last; }
};

// "N" or "N-M" (N <= M); false for anything else. An empty text is off.
inline bool ParseRange(const char* text, Range* out)
{
    *out = Range();
    if (!text || !*text)
        return true;
    char* end = nullptr;
    if (*text < '0' || *text > '9')
        return false;
    const unsigned long first = std::strtoul(text, &end, 10);
    unsigned long last = first;
    if (*end == '-') {
        const char* second = end + 1;
        if (*second < '0' || *second > '9')
            return false;
        last = std::strtoul(second, &end, 10);
    }
    if (*end != '\0' || last < first || last > 0xFFFFFFFFul)
        return false;
    out->on = true;
    out->first = static_cast<unsigned>(first);
    out->last = static_cast<unsigned>(last);
    return true;
}

struct Object {
    int index = 0;
    int32_t id = 0;
    unsigned type = 0;
    int32_t x = 0;
    int32_t y = 0;
    int here = 0;
    unsigned list = 0;
    int seen = 0;
    unsigned speed = 0;
    unsigned inhibitor = 0;
    bool hasName = false;
    std::string name;
    bool sprite = false;
    unsigned lastCharacters = 0;
    int32_t active = 0;
    int32_t activeAi = 0;
    int32_t activeImprisonment = 0;
    int32_t visibleMonster = 0;
    unsigned field294 = 0;
    uint32_t generalState = 0;
    int action = 0;
    int sequence = 0;
    int queued = 0;
    int queuedHead = -1;
    int triggers = 0;
    int timedEffects = 0;
    int equipedEffects = 0;
    uint32_t xp = 0;
    int hp = 0;
    int32_t destX = 0;
    int32_t destY = 0;
    // m_pPath[0 .. m_nPath - 1] (search positions), at most 64.
    std::vector<int32_t> path;
    // The m_timers list, head to tail: (id, time).
    std::vector<std::pair<unsigned, int32_t>> timers;
};

struct Area {
    bool present = false;
    std::string resref;
    unsigned characters = 0;
    int32_t viewX = 0;
    int32_t viewY = 0;
    int width = 0;
    int height = 0;
    int32_t size = 0;
    int range = 0;
    int32_t ids[6] = {};
    std::vector<uint8_t> map; // m_nMapSize bytes, empty when unreadable
};

struct Frame {
    unsigned frame = 0;
    std::string screen;
    int characters = 0;
    unsigned visibleArea = 0;
    int32_t slots[6] = { -1, -1, -1, -1, -1, -1 };
    int32_t portraits[6] = { -1, -1, -1, -1, -1, -1 };
    uint32_t gameTime = 0;
    unsigned timeActive = 0;
    Area area;
    std::vector<Object> objects;
};

// The type bytes that add a name (CGameObject::TYPE_DOOR, TYPE_SPRITE).
constexpr unsigned kTypeDoor = 0x21;
constexpr unsigned kTypeSprite = 0x31;

inline std::string Format(const Frame& f)
{
    std::string out;
    char line[512];
    std::snprintf(line, sizeof(line), "frame %u screen %s\n", f.frame, f.screen.c_str());
    out += line;
    std::snprintf(line, sizeof(line),
        "game characters %d visible_area %u slots %ld,%ld,%ld,%ld,%ld,%ld portraits %ld,%ld,%ld,%ld,%ld,%ld time %lu active %u\n",
        f.characters, f.visibleArea, static_cast<long>(f.slots[0]), static_cast<long>(f.slots[1]),
        static_cast<long>(f.slots[2]), static_cast<long>(f.slots[3]), static_cast<long>(f.slots[4]),
        static_cast<long>(f.slots[5]), static_cast<long>(f.portraits[0]), static_cast<long>(f.portraits[1]),
        static_cast<long>(f.portraits[2]), static_cast<long>(f.portraits[3]), static_cast<long>(f.portraits[4]),
        static_cast<long>(f.portraits[5]), static_cast<unsigned long>(f.gameTime), f.timeActive);
    out += line;
    const Area& a = f.area;
    if (a.present) {
        std::snprintf(line, sizeof(line),
            "area %s characters %u view %ld,%ld vis %dx%d size %ld range %d ids %ld,%ld,%ld,%ld,%ld,%ld\n",
            a.resref.c_str(), a.characters, static_cast<long>(a.viewX), static_cast<long>(a.viewY), a.width,
            a.height, static_cast<long>(a.size), a.range, static_cast<long>(a.ids[0]), static_cast<long>(a.ids[1]),
            static_cast<long>(a.ids[2]), static_cast<long>(a.ids[3]), static_cast<long>(a.ids[4]),
            static_cast<long>(a.ids[5]));
        out += line;
    }
    for (const Object& o : f.objects) {
        std::snprintf(line, sizeof(line), "obj %d %ld type %u pos %ld,%ld here %d list %u seen %d speed %u inhibitor %u",
            o.index, static_cast<long>(o.id), o.type, static_cast<long>(o.x), static_cast<long>(o.y), o.here, o.list,
            o.seen, o.speed, o.inhibitor);
        out += line;
        if (o.hasName)
            out += " name " + o.name;
        if (o.sprite) {
            std::snprintf(line, sizeof(line), " last %u active %ld ai %ld imprisonment %ld visible_monster %ld field_294 %u state %08lx"
                                              " action %d sequence %d queued %d next %d triggers %d effects %d,%d xp %lu hp %d",
                o.lastCharacters, static_cast<long>(o.active), static_cast<long>(o.activeAi),
                static_cast<long>(o.activeImprisonment), static_cast<long>(o.visibleMonster), o.field294,
                static_cast<unsigned long>(o.generalState), o.action, o.sequence, o.queued,
                o.queuedHead, o.triggers, o.timedEffects, o.equipedEffects, static_cast<unsigned long>(o.xp), o.hp);
            out += line;
            std::snprintf(line, sizeof(line), " dest %ld,%ld path", static_cast<long>(o.destX), static_cast<long>(o.destY));
            out += line;
            for (int32_t node : o.path) {
                std::snprintf(line, sizeof(line), " %ld", static_cast<long>(node));
                out += line;
            }
            out += " timers";
            for (const auto& timer : o.timers) {
                std::snprintf(line, sizeof(line), " %u:%ld", timer.first, static_cast<long>(timer.second));
                out += line;
            }
        }
        out += "\n";
    }
    if (a.present && a.width > 0) {
        static const char kHex[] = "0123456789abcdef";
        const size_t width = static_cast<size_t>(a.width);
        for (size_t row = 0; (row + 1) * width <= a.map.size(); ++row) {
            out += "vis " + std::to_string(row) + " ";
            for (size_t i = row * width; i < (row + 1) * width; ++i) {
                out += kHex[a.map[i] >> 4];
                out += kHex[a.map[i] & 0xF];
            }
            out += "\n";
        }
    }
    return out;
}

}
