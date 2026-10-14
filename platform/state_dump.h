// Our engine's replay state dump: the JSON the replay recorder writes from
// the original (oracle/replay/schema.json, oracle/replay/recorder.cpp's
// DumpJson), written from our engine in the deterministic replay mode
// (platform/replay.h) so tools/oracle.py replay compare can diff the two
// field by field.
//
// The platform layer does not know the engine's classes: the engine
// registers a DumpSource (src/StateDumpSources.cpp) that reads them, and the
// replay mode adds what it owns (the frame, the checkpoint, the rand states,
// the stall count and the frame gate's counts).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform::state_dump {

// schema.json "chargen".
struct Chargen {
    // False while the screen's m_nGameSprite does not resolve: race,
    // subrace, class, abilities, feats and portrait are then null.
    bool sprite = false;
    int race = 0;
    int subrace = 0;
    int klass = 0;
    int abilities[6] = {};
    int rolls[7] = {};
    int skillPoints = 0;
    uint32_t feats[3] = {};
    std::string portrait;
};

// One schema.json "world.party" entry.
struct PartyMember {
    int32_t id = 0;
    // False while the id does not resolve in the object array: x, y, hp,
    // max_hp, skills, action, modal and facing are then null.
    bool sprite = false;
    int32_t x = 0;
    int32_t y = 0;
    int hp = 0;
    int maxHp = 0;
    int skills[64] = {};
    int action = 0;
    int modal = 0;
    int facing = 0;
};

// One schema.json "world.doors" entry.
struct Door {
    std::string name;
    int open = 0;
    int locked = 0;
};

// schema.json "world.dialog".
struct Dialog {
    int active = 0;
    // -1 while not active.
    int64_t speaker = -1;
    int64_t entry = -1;
    int responses = 0;
};

// schema.json "world".
struct World {
    // "" when there is no area on view.
    std::string area;
    // At most 6, in portrait order.
    std::vector<PartyMember> party;
    // The area's doors in its front list's order.
    std::vector<Door> doors;
    Dialog dialog;
    int64_t exploredCount = 0;
    int64_t creatures = 0;
};

// What the engine's DumpSource reads.
struct EngineState {
    // schema.json "screen": none, unknown or the m_pEngine* member's name.
    std::string screen = "none";
    int32_t auCounter = 0;
    int fullscreen = 0;
    int32_t inAsyncUpdate = 0;
    int reinitializing = 0;
    // CChitin::cProgressBar.m_bProgressBarActivated (schema.json "loading").
    int loading = 0;
    // Only on the create_char screen.
    bool hasChargen = false;
    Chargen chargen;
    // Only on the world screen.
    bool hasWorld = false;
    World world;
};

// One dump.
struct Snapshot {
    uint32_t frame = 0;
    bool hasCheckpoint = false;
    std::string checkpoint;
    uint32_t rngState = 0;
    uint32_t aiStalledFrames = 0;
    // platform/replay_clock.h FrameGate: Loads() and HeldFrames().
    uint32_t loads = 0;
    uint32_t heldFrames = 0;
    // Null until the input thread has polled once.
    bool hasAiRngState = false;
    uint32_t aiRngState = 0;
    EngineState engine;
};

// What the frame gate and the stall count read on every call of the frame.
struct AiState {
    int32_t auCounter = 0;
    int32_t inAsyncUpdate = 0;
    int loading = 0;
};

// The engine's side, registered once at start-up. Read runs on the main
// thread, at the start of a frame, while the AI thread waits. ReadAi runs
// at the start of every call of the frame, held ones included, while the
// AI thread may be inside an update: it reads only the three members, in
// FrameGate::Counts's order.
class DumpSource {
public:
    virtual ~DumpSource();
    virtual void Read(EngineState* out) const = 0;
    virtual void ReadAi(AiState* out) const = 0;
    // False while the engine has no CBaldurChitin (g_pBaldurChitin null):
    // Read then reports nothing, and the stall count skips the frame.
    virtual bool Ready() const = 0;
};

// Registers the source (not owned); nullptr removes it.
void SetSource(const DumpSource* source);
const DumpSource* Source();

// The dump's JSON text, laid out as the recorder writes it.
std::string ToJson(const Snapshot& snapshot);

// Writes ToJson(snapshot) to path through a temporary file and a rename,
// so a reader never sees half of it. False when it cannot.
bool Write(const char* path, const Snapshot& snapshot);

// Writes text to path the same way.
bool WriteFileAtomically(const std::string& path, const std::string& text);

}
