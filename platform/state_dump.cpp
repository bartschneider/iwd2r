#include "platform/state_dump.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace platform::state_dump {

DumpSource::~DumpSource() = default;

namespace {

    std::atomic<const DumpSource*> g_source { nullptr };

    // recorder.cpp's JsonString: quotes and backslashes escaped, every byte
    // outside printable ASCII as \u00XX.
    std::string JsonString(const std::string& text)
    {
        std::string out = "\"";
        for (const char ch : text) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (c == '"' || c == '\\') {
                out += '\\';
                out += static_cast<char>(c);
            } else if (c < 0x20 || c >= 0x7F) {
                char escaped[8];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                out += escaped;
            } else {
                out += static_cast<char>(c);
            }
        }
        return out + "\"";
    }

    std::string Number(long long value)
    {
        return std::to_string(value);
    }

    template <typename T, size_t N>
    std::string List(const T (&values)[N])
    {
        std::string out = "[";
        for (size_t i = 0; i < N; ++i)
            out += (i ? ", " : "") + Number(static_cast<long long>(values[i]));
        return out + "]";
    }

    std::string ChargenJson(const Chargen& c)
    {
        const std::string null = "null";
        return "{\"race\": " + (c.sprite ? Number(c.race) : null)
            + ", \"subrace\": " + (c.sprite ? Number(c.subrace) : null)
            + ", \"class\": " + (c.sprite ? Number(c.klass) : null)
            + ", \"abilities\": " + (c.sprite ? List(c.abilities) : null)
            + ", \"rolls\": " + List(c.rolls)
            + ", \"skill_points\": " + Number(c.skillPoints)
            + ", \"feats\": " + (c.sprite ? List(c.feats) : null)
            + ", \"portrait\": " + (c.sprite ? JsonString(c.portrait) : null) + "}";
    }

    std::string WorldJson(const World& w)
    {
        const std::string null = "null";
        std::string party = "[";
        for (size_t i = 0; i < w.party.size(); ++i) {
            const PartyMember& m = w.party[i];
            party += std::string(i ? ", " : "") + "{\"id\": " + Number(m.id)
                + ", \"x\": " + (m.sprite ? Number(m.x) : null) + ", \"y\": " + (m.sprite ? Number(m.y) : null)
                + ", \"hp\": " + (m.sprite ? Number(m.hp) : null)
                + ", \"max_hp\": " + (m.sprite ? Number(m.maxHp) : null)
                + ", \"skills\": " + (m.sprite ? List(m.skills) : null)
                + ", \"action\": " + (m.sprite ? Number(m.action) : null)
                + ", \"modal\": " + (m.sprite ? Number(m.modal) : null)
                + ", \"facing\": " + (m.sprite ? Number(m.facing) : null)
                + ", \"morale\": " + (m.sprite ? Number(m.morale) : null) + "}";
        }
        party += "]";
        std::string enemies = "[";
        for (size_t i = 0; i < w.enemyCreatures.size(); ++i) {
            const EnemyCreature& e = w.enemyCreatures[i];
            enemies += std::string(i ? ", " : "") + "{\"id\": " + Number(e.id)
                + ", \"hp\": " + Number(e.hp) + ", \"max_hp\": " + Number(e.maxHp)
                + ", \"timed_effects\": " + Number(e.timedEffects)
                + ", \"equiped_effects\": " + Number(e.equipedEffects) + "}";
        }
        enemies += "]";
        std::string doors = "[";
        for (size_t i = 0; i < w.doors.size(); ++i) {
            const Door& d = w.doors[i];
            doors += std::string(i ? ", " : "") + "{\"name\": " + JsonString(d.name) + ", \"open\": " + Number(d.open)
                + ", \"locked\": " + Number(d.locked) + "}";
        }
        doors += "]";
        const Dialog& g = w.dialog;
        const std::string dialog = "{\"active\": " + Number(g.active) + ", \"speaker\": " + Number(g.speaker)
            + ", \"entry\": " + Number(g.entry) + ", \"responses\": " + Number(g.responses) + "}";
        return "{\"area\": " + JsonString(w.area) + ", \"party\": " + party + ", \"doors\": " + doors
            + ", \"dialog\": " + dialog + ", \"explored_count\": " + Number(w.exploredCount)
            + ", \"creatures\": " + Number(w.creatures)
            + ", \"enemy_creatures\": " + enemies + "}";
    }

    // recorder.cpp's GameJson: one member's values, or nulls without a
    // sprite; an empty slot is null.
    std::string GameMemberJson(const GameMember& m)
    {
        if (!m.sprite) {
            return "{\"id\": " + Number(m.id)
                + ", \"gold\": null, \"xp\": null, \"level\": null, \"class_levels\": null, \"inventory\": null}";
        }
        std::string inventory = "[";
        for (size_t i = 0; i < 51; ++i) {
            inventory += i ? ", " : "";
            if (!m.hasItem[i]) {
                inventory += "null";
                continue;
            }
            const Item& item = m.items[i];
            inventory += "{\"resref\": " + JsonString(item.resref) + ", \"uses\": " + List(item.uses)
                + ", \"flags\": " + Number(item.flags) + "}";
        }
        inventory += "]";
        return "{\"id\": " + Number(m.id) + ", \"gold\": " + Number(m.gold) + ", \"xp\": " + Number(m.xp)
            + ", \"level\": " + Number(m.level) + ", \"class_levels\": " + List(m.classLevels)
            + ", \"inventory\": " + inventory + "}";
    }

    std::string GameJson(const Game& g)
    {
        std::string members = "[";
        for (size_t i = 0; i < g.members.size(); ++i)
            members += (i ? ", " : "") + GameMemberJson(g.members[i]);
        members += "]";
        return "{\"party_gold\": " + Number(g.partyGold) + ", \"members\": " + members + "}";
    }

    // recorder.cpp's SlotsJson.
    std::string SlotsJson(const std::vector<Slot>& slots)
    {
        std::string out = "[";
        for (size_t i = 0; i < slots.size(); ++i) {
            const Slot& s = slots[i];
            out += std::string(i ? ", " : "") + "{\"file\": " + JsonString(s.file) + ", \"name\": " + JsonString(s.name)
                + ", \"character\": " + JsonString(s.character) + ", \"time\": " + Number(s.time)
                + ", \"chapter\": " + Number(s.chapter) + ", \"chapter_text\": " + JsonString(s.chapterText) + "}";
        }
        return out + "]";
    }

}

void SetSource(const DumpSource* source)
{
    g_source.store(source);
}

const DumpSource* Source()
{
    return g_source.load();
}

std::string ToJson(const Snapshot& s)
{
    const EngineState& e = s.engine;
    std::string json = "{\n  \"frame\": " + Number(s.frame) + ",\n";
    if (s.hasCheckpoint)
        json += "  \"checkpoint\": " + JsonString(s.checkpoint) + ",\n";
    json += "  \"screen\": " + JsonString(e.screen) + ",\n";
    json += "  \"rng_state\": " + Number(s.rngState) + ",\n";
    json += "  \"globals\": {\"au_counter\": " + Number(e.auCounter) + ", \"fullscreen\": " + Number(e.fullscreen)
        + ", \"in_async_update\": " + Number(e.inAsyncUpdate) + ", \"ai_stalled_frames\": " + Number(s.aiStalledFrames)
        + ", \"ai_overlap_frames\": " + Number(s.aiOverlapFrames)
        + ", \"search_wait_timeouts\": " + Number(s.searchWaitTimeouts)
        + ", \"reinitializing\": " + Number(e.reinitializing) + ", \"loading\": " + Number(e.loading)
        + ", \"loads\": " + Number(s.loads) + ", \"held_frames\": " + Number(s.heldFrames)
        + ", \"ai_rng_state\": " + (s.hasAiRngState ? Number(s.aiRngState) : std::string("null")) + "},\n";
    json += "  \"chargen\": " + (e.hasChargen ? ChargenJson(e.chargen) : std::string("null")) + ",\n";
    json += "  \"world\": " + (e.hasWorld ? WorldJson(e.world) : std::string("null")) + ",\n";
    json += "  \"game\": " + (e.hasGame ? GameJson(e.game) : std::string("null")) + ",\n";
    json += "  \"slots\": " + (e.hasSlots ? SlotsJson(e.slots) : std::string("null")) + "\n}\n";
    return json;
}

bool WriteFileAtomically(const std::string& path, const std::string& text)
{
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out << text;
        if (!out.flush())
            return false;
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    return !ec;
}

bool Write(const char* path, const Snapshot& snapshot)
{
    return path && WriteFileAtomically(path, ToJson(snapshot));
}

}
