// Message marshal/unmarshal pairs that no scenario reaches (section 3 of
// task-34-report.md, rows 21-40). Every case here is pending with a body:
// the fork has none of these five bodies (no CMessageProjectileTrailingVFX
// or CMessageContinueDialog class at all, no Marshal/Unmarshal body for
// CMessageSetVariable and CMessageLoadDialog), so each case pins the
// original's behavior on its own — the same original-only determinism as
// rules/dstats_ctor_zeroes_decrementing — until the import lands.
//
// The engine state these functions touch is one pointer chain:
// g_pBaldurChitin (0x8CF6DC) -> m_pObjectGame (+0x1C54) -> the
// CGameObjectArray embedded at +0x372C and the CGameRemoteObjectArray at
// +0x375A. The fixture installs a zeroed chitin and a zeroed game block
// covering both arrays, with only the game pointer linked (the same M1-3
// shape MakeFixture builds in rules.cpp). With both arrays zero:
//   - GetShare (0x599A50) runs unlocked (its threadNum byte at 0x851873 is
//     0) and the word counts at this+0x28/+0x2A are 0, so every id misses
//     without touching the null bucket table and it returns 3 (0x599C4D) —
//     never 0 (hit) and never 4/5 (the marshal's retry codes). One shape is
//     excluded: an id whose index word is 0 and whose low word is positive
//     would walk the null table, so the sweeps keep index words >= 1 (0 and
//     0xFFFFFFFF are special-cased misses on their own).
//   - Find (0x59B300) reads the bucket count at this+4, which is 0, so the
//     walk is skipped for every pair and it returns 0 — no id is found.
// Marshals therefore write their miss defaults ((player, remote) = (0,
// 0xFFFFFFFF), the word at 0x851878) and unmarshals store INVALID ids; both
// sides stay deterministic on original-only checks.
//
// Wire conventions pinned by the disassembly: unmarshals read their payload
// at pData+3 (the engine's own 3-byte spec header, 0x85E670) and take dwSize
// including those bytes; a marshal's own dwSize is payload-only. The cases
// build both exactly, so no cnt/dwSize assert (0x780C00) can fire. The
// TrailingVFX resref passes through CResRef::operator= (0x78ACC0), which
// zero-fills its 8 bytes, copies min(len, 8) and uppercases a-z on the way;
// fixtures stay in [A-Z0-9] except where a case pins the transform itself.

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

using TrailingVfxMarshalFn = void(__fastcall*)(void* self, void* edx, uint8_t** ppData, uint32_t* dwSize);
constexpr uintptr_t kTrailingVfxMarshal = 0x511D00;
using UnmarshalFn = int32_t(__fastcall*)(void* self, void* edx, const uint8_t* pData, uint32_t dwSize);
constexpr uintptr_t kTrailingVfxUnmarshal = 0x511F40;
constexpr uintptr_t kSetVariableUnmarshal = 0x50A6C0;
constexpr uintptr_t kContinueDialogUnmarshal = 0x4FDD30;
constexpr uintptr_t kLoadDialogUnmarshal = 0x502D60;

constexpr uintptr_t kBaldurChitin = 0x8CF6DC;
constexpr size_t kObjectGameOffset = 0x1C54;
constexpr size_t kRemoteArrayOffset = 0x375A;
constexpr size_t kGameBlockSize = kRemoteArrayOffset + 8;

constexpr uint32_t kInvalidId = 0xFFFFFFFF;
constexpr uint8_t kPoison = 0xCD;
constexpr uint32_t kPoisonDword = 0xCDCDCDCD;

// Largest member offset each message type accesses (+1), so a fixture block
// covers every read and write: TrailingVFX reads a WORD at +0x28,
// ContinueDialog a BYTE at +0x30, LoadDialog a LONG at +0x10, SetVariable a
// DWORD at +0x1A.
constexpr size_t kTrailingVfxSize = 0x2C;
constexpr size_t kSetVariableSize = 0x1E;
constexpr size_t kContinueDialogSize = 0x34;
constexpr size_t kLoadDialogSize = 0x14;

uint8_t* OriginalZeroBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

// The poison doctrine from rules/dstats_ctor_zeroes_decrementing: a member a
// function leaves untouched shows the poison, not the luck of a zeroing
// allocator. Blocks are never freed; the oracle leaks them on purpose.
uint8_t* OriginalPoisonBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, kPoison, size);
    return at;
}

// Constructs an empty CString through the original's own constructor, the
// same fixture trick rules.cpp uses: operator= later releases that empty
// buffer, exactly what the engine does to a real message member.
void OriginalCString(void* at, const std::string& text)
{
    reinterpret_cast<OriginalCStringFn>(kOriginalCString)(at, nullptr, text.c_str());
}

// The M1-3 fixture: zeroed chitin and game block (both embedded arrays are
// zero), only the object-game pointer linked. Must be built inside
// ctx.CallOriginal — it allocates on the original heap. The runner's .data
// snapshot restores g_pBaldurChitin at the next case boundary; the
// destructor keeps the window tight anyway.
struct OriginalGame {
    void Install()
    {
        chitin = OriginalZeroBlock(kObjectGameOffset + 4);
        game = OriginalZeroBlock(kGameBlockSize);
        uint32_t gameAt = 0;
        std::memcpy(&gameAt, &game, sizeof(gameAt));
        std::memcpy(chitin + kObjectGameOffset, &gameAt, sizeof(gameAt));
        std::memcpy(reinterpret_cast<void*>(kBaldurChitin), &chitin, sizeof(chitin));
    }

    ~OriginalGame()
    {
        uint8_t* const clear = nullptr;
        std::memcpy(reinterpret_cast<void*>(kBaldurChitin), &clear, sizeof(clear));
    }

    uint8_t* chitin = nullptr;
    uint8_t* game = nullptr;
};

// Little-endian wire builder. The append order of each case mirrors the
// disassembly-pinned layout, header first.
struct Wire {
    void U8(uint8_t value) { bytes.push_back(static_cast<char>(value)); }
    void U16(uint16_t value)
    {
        for (int i = 0; i < 2; ++i) {
            U8(static_cast<uint8_t>(value >> (8 * i)));
        }
    }
    void U32(uint32_t value)
    {
        for (int i = 0; i < 4; ++i) {
            U8(static_cast<uint8_t>(value >> (8 * i)));
        }
    }
    void S16(int16_t value) { U16(static_cast<uint16_t>(value)); }
    void Raw(const std::string& value) { bytes += value; }
    // The three bytes the engine's spec header occupies: every unmarshal
    // starts its payload at pData+3.
    void Header() { bytes.append(3, '\0'); }
    void Pair(uint32_t player, uint32_t remote)
    {
        U32(player);
        U32(remote);
    }

    std::string bytes;
};

std::string ReadBytes(const uint8_t* at, size_t count)
{
    return std::string(reinterpret_cast<const char*>(at), count);
}

uint32_t ReadU32(const uint8_t* at)
{
    uint32_t value = 0;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

uint16_t ReadU16(const uint8_t* at)
{
    uint16_t value = 0;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

// The original CString: a buffer pointer whose length word lives at -8 —
// the engine's own marshal reads it there (0x511DF6) — with the bytes at
// the pointer. Anything non-positive reads as the empty string.
std::string ReadCString(const uint8_t* member)
{
    const char* data = nullptr;
    std::memcpy(&data, member, sizeof(data));
    if (data == nullptr) {
        return std::string();
    }
    int32_t length = 0;
    std::memcpy(&length, data - 8, sizeof(length));
    return length > 0 ? std::string(data, static_cast<size_t>(length)) : std::string();
}

// The 8 zero-padded bytes a CResRef member holds: what CResRef::operator=
// leaves and what the marshal's CopyToString reads. An unpadded store would
// leave garbage behind a short string, and strlen of that garbage would
// change the marshaled length.
std::string ResrefField(const std::string& resref)
{
    std::string field = resref.substr(0, resref.size() < 8 ? resref.size() : 8);
    field.resize(8, '\0');
    return field;
}

// CResRef::operator= (0x78ACC0): zero-fill 8 bytes, copy min(len, 8),
// uppercase a-z.
std::string ExpectedResref(const std::string& wireBytes)
{
    std::string member(8, '\0');
    const size_t count = wireBytes.size() < 8 ? wireBytes.size() : 8;
    for (size_t i = 0; i < count; ++i) {
        char c = wireBytes[i];
        if (c > '\x60' && c < '\x7b') {
            c = static_cast<char>(c - 0x20);
        }
        member[i] = c;
    }
    return member;
}

std::string Hex(const std::string& value)
{
    static const char* kDigits = "0123456789ABCDEF";
    std::string out;
    for (char c : value) {
        out += kDigits[static_cast<unsigned char>(c) >> 4];
        out += kDigits[static_cast<unsigned char>(c) & 0xF];
    }
    return out;
}

std::string Hex(uint32_t value)
{
    static const char* kDigits = "0123456789ABCDEF";
    std::string out = "0x";
    for (int i = 7; i >= 0; --i) {
        out += kDigits[(value >> (4 * i)) & 0xF];
    }
    return out;
}

std::string Hex(uint16_t value)
{
    static const char* kDigits = "0123456789ABCDEF";
    std::string out = "0x";
    for (int i = 3; i >= 0; --i) {
        out += kDigits[(value >> (4 * i)) & 0xF];
    }
    return out;
}

// Same shape as rules.cpp: seeded per case, salted per use.
struct Picks {
    Picks(uint64_t seed, uint64_t salt)
        : state(seed ^ salt)
        , bits(oracle::SplitMix64(&state))
    {
    }
    uint32_t Bits() { return *bits.Next(); }
    int32_t Between(int32_t lo, int32_t hi)
    {
        const uint32_t span = static_cast<uint32_t>(hi) - static_cast<uint32_t>(lo) + 1u;
        return static_cast<int32_t>(static_cast<uint32_t>(lo) + (span ? Bits() % span : Bits()));
    }

    uint64_t state;
    oracle::RandomGen<uint32_t> bits;
};

const char* const kAlphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

std::string PickText(Picks& picks, size_t minLength, size_t maxLength)
{
    std::string text;
    const size_t length = minLength + picks.Bits() % static_cast<uint32_t>(maxLength - minLength + 1);
    text.resize(length);
    for (char& c : text) {
        c = kAlphabet[picks.Bits() % 62];
    }
    return text;
}

// ------- messages/511D00 projectile trailing VFX marshal

// CMessageProjectileTrailingVFX::MarshalMessage: GetShare(this+4) misses, so
// the wire opens with the (0, INVALID) default pair, then CopyToString
// (0x78A9F0) trims the raw 8 resref bytes to the first NUL (R), the name
// CString at +0x14 follows (N), then the four DWORDs at +0x18/+0x1C/+0x20/
// +0x24 and the WORD at +0x28; dwSize = 0x1C + R + N.
void TrailingVfxMarshal(CaseContext& ctx)
{
    struct Input {
        std::string resref; // raw 8 bytes, embedded NULs allowed
        std::string name;
        uint32_t tail[4];
        uint16_t last;
        uint32_t spriteId;
    };
    const Input inputs[] = {
        {"SPFIR001", "TRAILING VFX", {0, 1, 0x7FFFFFFF, 0xFFFFFFFF}, 0x1234, 0x0001ABCD},
        {"ABCD1234", "", {42, 0x80000000, 7, 0}, 0, 0},
        {"", "A", {0, 0, 0, 0}, 0xFFFF, 0xFFFFFFFF},
        {std::string("SPF\0IR\0", 6), "Trimmed", {1, 2, 3, 4}, 0x0102, 0},
        {"01234567", "x", {0xDEADBEEF, 0, 0, 0xCAFEF00D}, 0x8000, 0x7FFF1234},
    };

    auto run = [&](const Input& in) {
        // The member is a CResRef: 8 zero-padded bytes. CopyToString
        // produces a CString whose length is the field's strlen, so the
        // stored shape - not the input string alone - decides R.
        const std::string field = ResrefField(in.resref);
        const size_t nul = field.find('\0');
        const std::string resrefRun = nul == std::string::npos ? field : field.substr(0, nul);

        Wire expected;
        expected.Pair(0, kInvalidId);
        expected.U8(static_cast<uint8_t>(resrefRun.size()));
        expected.Raw(resrefRun);
        expected.U8(static_cast<uint8_t>(in.name.size()));
        expected.Raw(in.name);
        for (uint32_t value : in.tail) {
            expected.U32(value);
        }
        expected.U16(in.last);

        ctx.SetInputs("sprite=" + Hex(in.spriteId) + " resref=" + Hex(in.resref) + " name=\"" + in.name
                      + "\" tail=[" + Hex(in.tail[0]) + "," + Hex(in.tail[1]) + "," + Hex(in.tail[2]) + ","
                      + Hex(in.tail[3]) + "] last=" + Hex(in.last));

        uint32_t actualSize = 0;
        std::string actualWire;
        uint8_t buffer[0x100];
        ctx.CallOriginal([&] {
            OriginalGame game;
            game.Install();

            uint8_t* message = OriginalPoisonBlock(kTrailingVfxSize);
            std::memcpy(message + 4, &in.spriteId, sizeof(in.spriteId));
            std::memcpy(message + 0xC, field.data(), field.size());
            OriginalCString(message + 0x14, in.name);
            std::memcpy(message + 0x18, in.tail, sizeof(in.tail));
            std::memcpy(message + 0x28, &in.last, sizeof(in.last));

            uint8_t* out = buffer;
            uint32_t size = 0;
            reinterpret_cast<TrailingVfxMarshalFn>(kTrailingVfxMarshal)(message, nullptr, &out, &size);
            actualSize = size;
            actualWire = ReadBytes(out, size);
        });
        ctx.CountInputs(1);

        oracle::Compare(ctx, expected.bytes.size(), actualSize);
        oracle::Compare(ctx, expected.bytes, actualWire);
    };

    for (const Input& in : inputs) {
        run(in);
    }

    Picks picks(ctx.seed, 0x547261696C566658ull /* "TrailVfX" */);
    const int rounds = ctx.iterations < 8 ? 8 : ctx.iterations;
    for (int round = 0; round < rounds; ++round) {
        Input in;
        in.resref = PickText(picks, 0, 8);
        in.name = PickText(picks, 0, 16);
        for (uint32_t& value : in.tail) {
            value = picks.Bits();
        }
        in.last = static_cast<uint16_t>(picks.Bits());
        // Index word >= 1 keeps GetShare off the null bucket table.
        in.spriteId = (((picks.Bits() % 0x7FFFu) + 1u) << 16) | picks.Bits();
        run(in);
    }
}

// ------- messages/511F40 projectile trailing VFX unmarshal

// CMessageProjectileTrailingVFX::UnmarshalMessage: pair at +3/+7 (Find
// misses, this+4 = INVALID), BYTE len at +0xB then the resref bytes into
// CResRef::operator= at this+0xC, BYTE len at +0xC+R then the name bytes
// into CString::operator= at this+0x14, four DWORDs at +0x18..+0x24, WORD at
// +0x28. No cnt assert. Both strings pass through 9-byte stack temps (8
// chars + NUL), so wires with R or N above 8 overrun the original's stack -
// the same unsafe-input class as GetShare's null bucket table, and the
// sweeps stay inside it (the original's own resrefs are 8 chars by
// definition; it round-trips the resref, and a name longer than 8 would
// break its own unmarshal). The case also closes the loop with the marshal:
// feeding a marshaled wire (with its 3 header bytes) back must restore every
// member except the sprite id, which the miss replaced with INVALID.
void TrailingVfxUnmarshal(CaseContext& ctx)
{
    struct Input {
        uint32_t player;
        uint32_t remote;
        std::string resref; // wire bytes, length R
        std::string name;
        uint32_t tail[4];
        uint16_t last;
    };
    const Input inputs[] = {
        {0, kInvalidId, "SPFIR001", "TRAILVFX", {0, 1, 0x7FFFFFFF, 0xFFFFFFFF}, 0x1234},
        {0x12345678, 0x9ABCDEF0, "ABCD1234", "", {42, 0x80000000, 7, 0}, 0},
        {0, kInvalidId, "", "A", {0, 0, 0, 0}, 0xFFFF},
        {0, kInvalidId, "spf!r001", "lowercs", {1, 2, 3, 4}, 0x0102}, // pins the a-z transform
        {0, kInvalidId, "01234567", "8charsok", {9, 9, 9, 9}, 9},     // R = N = 8, full width
    };

    auto run = [&](const Input& in) {
        Wire wire;
        wire.Header();
        wire.Pair(in.player, in.remote);
        wire.U8(static_cast<uint8_t>(in.resref.size()));
        wire.Raw(in.resref);
        wire.U8(static_cast<uint8_t>(in.name.size()));
        wire.Raw(in.name);
        for (uint32_t value : in.tail) {
            wire.U32(value);
        }
        wire.U16(in.last);

        ctx.SetInputs("pair=(" + Hex(in.player) + "," + Hex(in.remote) + ") resref=" + Hex(in.resref)
                      + " name=\"" + in.name + "\"");

        int32_t returned = 0;
        uint8_t* message = nullptr;
        ctx.CallOriginal([&] {
            OriginalGame game;
            game.Install();

            message = OriginalPoisonBlock(kTrailingVfxSize);
            OriginalCString(message + 0x14, ""); // operator= releases it
            returned = reinterpret_cast<UnmarshalFn>(kTrailingVfxUnmarshal)(
                message, nullptr, reinterpret_cast<const uint8_t*>(wire.bytes.data()),
                static_cast<uint32_t>(wire.bytes.size()));
        });
        ctx.CountInputs(1);

        oracle::Compare(ctx, 1, returned);
        oracle::Compare(ctx, kInvalidId, ReadU32(message + 4));
        oracle::Compare(ctx, ExpectedResref(in.resref), ReadBytes(message + 0xC, 8));
        oracle::Compare(ctx, in.name, ReadCString(message + 0x14));
        oracle::Compare(ctx, in.tail[0], ReadU32(message + 0x18));
        oracle::Compare(ctx, in.tail[1], ReadU32(message + 0x1C));
        oracle::Compare(ctx, in.tail[2], ReadU32(message + 0x20));
        oracle::Compare(ctx, in.tail[3], ReadU32(message + 0x24));
        oracle::Compare(ctx, in.last, ReadU16(message + 0x28));
    };

    for (const Input& in : inputs) {
        run(in);
    }

    Picks picks(ctx.seed, 0x556E6D6172566658ull /* "UnmarVfX" */);
    const int rounds = ctx.iterations < 8 ? 8 : ctx.iterations;
    for (int round = 0; round < rounds; ++round) {
        Input in;
        in.player = picks.Bits();
        in.remote = picks.Bits();
        in.resref = PickText(picks, 0, 8);
        in.name = PickText(picks, 0, 8);
        for (uint32_t& value : in.tail) {
            value = picks.Bits();
        }
        in.last = static_cast<uint16_t>(picks.Bits());
        run(in);
    }

    // Marshal -> unmarshal round trip. The resref stays full width ([A-Z0-9]
    // only) so the trim and the transform cancel out.
    struct Trip {
        std::string resref;
        std::string name;
        uint32_t tail[4];
        uint16_t last;
    };
    const Trip trips[] = {
        {"SPFIR001", "TRAILVFX", {0, 1, 0x7FFFFFFF, 0xFFFFFFFF}, 0x1234},
        {"", "RoundTri", {0xDEADBEEF, 0, 0, 0xCAFEF00D}, 0x8000},
        {"01234567", "", {1, 2, 3, 4}, 0},
    };
    for (const Trip& trip : trips) {
        ctx.SetInputs("round trip resref=" + Hex(trip.resref) + " name=\"" + trip.name + "\"");

        uint8_t* unmarshaled = nullptr;
        ctx.CallOriginal([&] {
            OriginalGame game;
            game.Install();

            const uint32_t spriteId = 0x0001ABCD;
            uint8_t* marshaled = OriginalPoisonBlock(kTrailingVfxSize);
            std::memcpy(marshaled + 4, &spriteId, sizeof(spriteId));
            const std::string field = ResrefField(trip.resref);
            std::memcpy(marshaled + 0xC, field.data(), field.size());
            OriginalCString(marshaled + 0x14, trip.name);
            std::memcpy(marshaled + 0x18, trip.tail, sizeof(trip.tail));
            std::memcpy(marshaled + 0x28, &trip.last, sizeof(trip.last));

            uint8_t buffer[0x100];
            uint8_t* out = buffer;
            uint32_t size = 0;
            reinterpret_cast<TrailingVfxMarshalFn>(kTrailingVfxMarshal)(marshaled, nullptr, &out, &size);
            const std::string wireBytes = std::string(3, '\0') + ReadBytes(out, size);

            unmarshaled = OriginalPoisonBlock(kTrailingVfxSize);
            OriginalCString(unmarshaled + 0x14, "");
            reinterpret_cast<UnmarshalFn>(kTrailingVfxUnmarshal)(
                unmarshaled, nullptr, reinterpret_cast<const uint8_t*>(wireBytes.data()),
                static_cast<uint32_t>(wireBytes.size()));
        });
        ctx.CountInputs(1);

        // The sprite id does not survive: the miss replaced it with INVALID.
        oracle::Compare(ctx, kInvalidId, ReadU32(unmarshaled + 4));
        oracle::Compare(ctx, ExpectedResref(trip.resref), ReadBytes(unmarshaled + 0xC, 8));
        oracle::Compare(ctx, trip.name, ReadCString(unmarshaled + 0x14));
        oracle::Compare(ctx, trip.tail[0], ReadU32(unmarshaled + 0x18));
        oracle::Compare(ctx, trip.tail[1], ReadU32(unmarshaled + 0x1C));
        oracle::Compare(ctx, trip.tail[2], ReadU32(unmarshaled + 0x20));
        oracle::Compare(ctx, trip.tail[3], ReadU32(unmarshaled + 0x24));
        oracle::Compare(ctx, trip.last, ReadU16(unmarshaled + 0x28));
    }
}

// ------- messages/50A6C0 set variable unmarshal

// CMessageSetVariable::UnmarshalMessage: pair at +3/+7 (Find misses, this+4
// = INVALID), SHORT len at +0xB then the name string into this+0x10
// (m_sName), SHORT len then the scope string into this+0xC (m_sScope), then
// the value DWORD to +0x14, the increment BYTE to +0x18 and the flags DWORD
// to +0x1A; cnt = 0x18 + len1 + len2. No abort path exists: only the first
// pair consults the array and a miss never stops this unmarshal.
void SetVariableUnmarshal(CaseContext& ctx)
{
    struct Input {
        uint32_t player;
        uint32_t remote;
        std::string name;    // wire string 1 -> this+0x10
        std::string scope;   // wire string 2 -> this+0xC
        uint32_t value;      // this+0x14
        uint8_t increment;   // this+0x18
        uint32_t flags;      // this+0x1A
    };
    const Input inputs[] = {
        {0, kInvalidId, "KAPUT", "GLOBAL", 1, 0, 0},
        {0x55667788, 0x99AABBCC, "", "", 0, 0, 0},
        {0, kInvalidId, "ARETHYA_VARIABLE", "LOCALS", 0x7FFFFFFF, 1, 0x0000000F},
        {0, kInvalidId, "forty chars of name padding 1234567", "scope too", 0xDEADBEEF, 0x80, 0xFFFFFFFF},
    };

    auto run = [&](const Input& in) {
        Wire wire;
        wire.Header();
        wire.Pair(in.player, in.remote);
        wire.S16(static_cast<int16_t>(in.name.size()));
        wire.Raw(in.name);
        wire.S16(static_cast<int16_t>(in.scope.size()));
        wire.Raw(in.scope);
        wire.U32(in.value);
        wire.U8(in.increment);
        wire.U32(in.flags);

        ctx.SetInputs("pair=(" + Hex(in.player) + "," + Hex(in.remote) + ") name=\"" + in.name
                      + "\" scope=\"" + in.scope + "\" value=" + Hex(in.value)
                      + " increment=" + Hex(static_cast<uint32_t>(in.increment)) + " flags=" + Hex(in.flags));

        int32_t returned = 0;
        uint8_t* message = nullptr;
        ctx.CallOriginal([&] {
            OriginalGame game;
            game.Install();

            message = OriginalPoisonBlock(kSetVariableSize);
            OriginalCString(message + 0xC, "");
            OriginalCString(message + 0x10, "");
            returned = reinterpret_cast<UnmarshalFn>(kSetVariableUnmarshal)(
                message, nullptr, reinterpret_cast<const uint8_t*>(wire.bytes.data()),
                static_cast<uint32_t>(wire.bytes.size()));
        });
        ctx.CountInputs(1);

        oracle::Compare(ctx, 1, returned);
        oracle::Compare(ctx, kInvalidId, ReadU32(message + 4));
        oracle::Compare(ctx, in.name, ReadCString(message + 0x10));
        oracle::Compare(ctx, in.scope, ReadCString(message + 0xC));
        oracle::Compare(ctx, in.value, ReadU32(message + 0x14));
        oracle::Compare(ctx, in.increment, message[0x18]);
        oracle::Compare(ctx, in.flags, ReadU32(message + 0x1A));
    };

    for (const Input& in : inputs) {
        run(in);
    }

    Picks picks(ctx.seed, 0x556E6D6172536574ull /* "UnmarSet" */);
    const int rounds = ctx.iterations < 8 ? 8 : ctx.iterations;
    for (int round = 0; round < rounds; ++round) {
        Input in;
        in.player = picks.Bits();
        in.remote = picks.Bits();
        in.name = PickText(picks, 0, 24);
        in.scope = PickText(picks, 0, 24);
        in.value = picks.Bits();
        in.increment = static_cast<uint8_t>(picks.Bits());
        in.flags = picks.Bits();
        run(in);
    }
}

// ------- messages/502D60 load dialog unmarshal

// CMessageLoadDialog::UnmarshalMessage: pair at +3/+7 (Find misses, this+4 =
// INVALID), SHORT len at +0xB then the dialog string into this+0xC, then the
// second pair: a miss of (0, INVALID) stores INVALID at this+0x10 and
// returns 1; any other miss returns 0 with this+0x10 untouched (the dialog
// string was already stored). cnt = 0x15 + len.
void LoadDialogUnmarshal(CaseContext& ctx)
{
    struct Input {
        uint32_t player1;
        uint32_t remote1;
        std::string dialog;
        uint32_t player2;
        uint32_t remote2;
        bool aborts;
    };
    const Input inputs[] = {
        {0, kInvalidId, "AEMYRA", 0, kInvalidId, false},
        {0x12345678, 0x9ABCDEF0, "", 0, kInvalidId, false},
        {0, kInvalidId, "0CHICK3N", 5, 6, true},
        {0, kInvalidId, "DLGNAME9", 0xCAFEBABE, 0, true},
    };

    auto run = [&](const Input& in) {
        Wire wire;
        wire.Header();
        wire.Pair(in.player1, in.remote1);
        wire.S16(static_cast<int16_t>(in.dialog.size()));
        wire.Raw(in.dialog);
        wire.Pair(in.player2, in.remote2);

        ctx.SetInputs("pair1=(" + Hex(in.player1) + "," + Hex(in.remote1) + ") dialog=\"" + in.dialog
                      + "\" pair2=(" + Hex(in.player2) + "," + Hex(in.remote2) + ")");

        int32_t returned = 0;
        uint8_t* message = nullptr;
        ctx.CallOriginal([&] {
            OriginalGame game;
            game.Install();

            message = OriginalPoisonBlock(kLoadDialogSize);
            OriginalCString(message + 0xC, "");
            returned = reinterpret_cast<UnmarshalFn>(kLoadDialogUnmarshal)(
                message, nullptr, reinterpret_cast<const uint8_t*>(wire.bytes.data()),
                static_cast<uint32_t>(wire.bytes.size()));
        });
        ctx.CountInputs(1);

        oracle::Compare(ctx, in.aborts ? 0 : 1, returned);
        oracle::Compare(ctx, kInvalidId, ReadU32(message + 4));
        // Stored before the second pair: present even on the abort path.
        oracle::Compare(ctx, in.dialog, ReadCString(message + 0xC));
        if (in.aborts) {
            oracle::Compare(ctx, kPoisonDword, ReadU32(message + 0x10));
        } else {
            oracle::Compare(ctx, kInvalidId, ReadU32(message + 0x10));
        }
    };

    for (const Input& in : inputs) {
        run(in);
    }

    Picks picks(ctx.seed, 0x556E6D61724C6F67ull /* "UnmarLog" */);
    const int rounds = ctx.iterations < 8 ? 8 : ctx.iterations;
    for (int round = 0; round < rounds; ++round) {
        Input in;
        in.player1 = picks.Bits();
        in.remote1 = picks.Bits();
        in.dialog = PickText(picks, 0, 31);
        in.player2 = picks.Bits();
        in.remote2 = picks.Bits();
        in.aborts = (picks.Bits() & 1) != 0;
        if (!in.aborts) {
            // The only non-aborting miss.
            in.player2 = 0;
            in.remote2 = kInvalidId;
        }
        run(in);
    }
}

// ------- messages/4FDD30 continue dialog unmarshal

// CMessageContinueDialog::UnmarshalMessage: pair at +3/+7 (Find misses,
// this+4 = INVALID), SHORT len1 at +0xB then string1 into this+0xC, three
// DWORDs into +0x10/+0x14/+0x18, the second pair into +0x1C, SHORT len2
// then string2 into +0x20, the third pair into +0x24, the fourth into +0x28
// (a miss there never aborts), a DWORD into +0x2C and a BYTE into +0x30;
// cnt = 0x38 + len1 + len2. The second pair's other-miss only skips its
// store (+0x1C keeps its old bytes) and the run continues; the third pair
// is the one that decides the return: hit or (0, INVALID) stores and runs
// the tail, any other miss returns 0 with +0x24 onward untouched.
void ContinueDialogUnmarshal(CaseContext& ctx)
{
    struct Input {
        uint32_t player1;
        uint32_t remote1;
        std::string text1;
        uint32_t values[3];
        uint32_t player2;
        uint32_t remote2;
        std::string text2;
        uint32_t player3;
        uint32_t remote3;
        uint32_t player4;
        uint32_t remote4;
        uint32_t value;
        uint8_t flag;
        int mode; // 0 = completes, 2 = second pair other-miss, 3 = third pair other-miss
    };
    const Input inputs[] = {
        {0, kInvalidId, "JOURNAL1", {1, 2, 3}, 0, kInvalidId, "NEXTDLG", 0, kInvalidId, 0x11111111,
         0x22222222, 0x33333333, 0x44, 0},
        {0x12345678, 0x9ABCDEF0, "", {0, 0x80000000, 0xFFFFFFFF}, 0, kInvalidId, "", 0, kInvalidId, 0,
         0, 0, 0, 0},
        // Pair 2 other-miss: the +0x1C store is skipped but the run
        // continues through the tail, so everything else still lands.
        {0, kInvalidId, "KEEPS OLD", {7, 8, 9}, 7, 8, "STILL SET", 0, kInvalidId, 0, 0, 0x5A, 0x5B, 2},
        // Pair 3 other-miss: return 0, +0x24 onward untouched (+0x1C and
        // text2 were stored before the miss).
        {0, kInvalidId, "SET TOO", {0xA, 0xB, 0xC}, 0, kInvalidId, "SET BEFORE PAIR3", 9, 10, 0x55, 0x66,
         0x77, 0x88, 3},
    };

    auto run = [&](const Input& in) {
        Wire wire;
        wire.Header();
        wire.Pair(in.player1, in.remote1);
        wire.S16(static_cast<int16_t>(in.text1.size()));
        wire.Raw(in.text1);
        for (uint32_t value : in.values) {
            wire.U32(value);
        }
        wire.Pair(in.player2, in.remote2);
        wire.S16(static_cast<int16_t>(in.text2.size()));
        wire.Raw(in.text2);
        wire.Pair(in.player3, in.remote3);
        wire.Pair(in.player4, in.remote4);
        wire.U32(in.value);
        wire.U8(in.flag);

        ctx.SetInputs("pair1=(" + Hex(in.player1) + "," + Hex(in.remote1) + ") text1=\"" + in.text1
                      + "\" pair2=(" + Hex(in.player2) + "," + Hex(in.remote2) + ") text2=\"" + in.text2
                      + "\" pair3=(" + Hex(in.player3) + "," + Hex(in.remote3) + ") pair4=("
                      + Hex(in.player4) + "," + Hex(in.remote4) + ") mode="
                      + (in.mode == 0 ? "full" : in.mode == 2 ? "pair2skip" : "pair3abort"));

        int32_t returned = 0;
        uint8_t* message = nullptr;
        ctx.CallOriginal([&] {
            OriginalGame game;
            game.Install();

            message = OriginalPoisonBlock(kContinueDialogSize);
            OriginalCString(message + 0xC, "");
            OriginalCString(message + 0x20, "");
            returned = reinterpret_cast<UnmarshalFn>(kContinueDialogUnmarshal)(
                message, nullptr, reinterpret_cast<const uint8_t*>(wire.bytes.data()),
                static_cast<uint32_t>(wire.bytes.size()));
        });
        ctx.CountInputs(1);

        oracle::Compare(ctx, in.mode == 3 ? 0 : 1, returned);
        oracle::Compare(ctx, kInvalidId, ReadU32(message + 4));
        oracle::Compare(ctx, in.text1, ReadCString(message + 0xC));
        oracle::Compare(ctx, in.values[0], ReadU32(message + 0x10));
        oracle::Compare(ctx, in.values[1], ReadU32(message + 0x14));
        oracle::Compare(ctx, in.values[2], ReadU32(message + 0x18));
        if (in.mode == 2) {
            // The other-miss skips the store: +0x1C keeps its poison.
            oracle::Compare(ctx, kPoisonDword, ReadU32(message + 0x1C));
        } else {
            oracle::Compare(ctx, kInvalidId, ReadU32(message + 0x1C));
        }
        // Text 2 lands even on the skipping and the aborting path.
        oracle::Compare(ctx, in.text2, ReadCString(message + 0x20));
        if (in.mode == 3) {
            oracle::Compare(ctx, kPoisonDword, ReadU32(message + 0x24));
            oracle::Compare(ctx, kPoisonDword, ReadU32(message + 0x28));
            oracle::Compare(ctx, kPoisonDword, ReadU32(message + 0x2C));
            oracle::Compare(ctx, kPoison, message[0x30]);
        } else {
            oracle::Compare(ctx, kInvalidId, ReadU32(message + 0x24));
            // Pair 4's miss never aborts: INVALID lands even after pair 3.
            oracle::Compare(ctx, kInvalidId, ReadU32(message + 0x28));
            oracle::Compare(ctx, in.value, ReadU32(message + 0x2C));
            oracle::Compare(ctx, in.flag, message[0x30]);
        }
    };

    for (const Input& in : inputs) {
        run(in);
    }

    Picks picks(ctx.seed, 0x556E6D6172436E67ull /* "UnmarCng" */);
    const int rounds = ctx.iterations < 8 ? 8 : ctx.iterations;
    for (int round = 0; round < rounds; ++round) {
        Input in;
        in.player1 = picks.Bits();
        in.remote1 = picks.Bits();
        in.text1 = PickText(picks, 0, 24);
        for (uint32_t& value : in.values) {
            value = picks.Bits();
        }
        in.player2 = 0;
        in.remote2 = kInvalidId;
        in.text2 = PickText(picks, 0, 24);
        in.player3 = 0;
        in.remote3 = kInvalidId;
        in.player4 = picks.Bits();
        in.remote4 = picks.Bits();
        in.value = picks.Bits();
        in.flag = static_cast<uint8_t>(picks.Bits());
        in.mode = 0;
        run(in);
    }
}

} // namespace

// Rows 21-40 of the section-3 table: all twenty rows are only_theirs, so
// every case is pending-with-body (original-only, like
// rules/dstats_ctor_zeroes_decrementing). The other fifteen rows read live
// engine state no case can shape: the marshals stop at their first GetShare
// miss before touching their argument domain (4F8D60, 4FD930, 5004D0,
// 504070, 50A4D0, 515010, 5154E0, 516910, 516D50, 502B40 — and 5017E0,
// whose live CAIResponse list is doubly engine-bound) and the unmarshals
// return 0 before reading the buffer (515180, 515670, 516A80, 501FE0).

ORACLE_CASE_PENDING("messages-511D00", TrailingVfxMarshal,
    "ours has no CMessageProjectileTrailingVFX class to marshal; the case pins the original's miss-path wire until the import lands");
ORACLE_CASE_PENDING("messages-511F40", TrailingVfxUnmarshal,
    "ours has no CMessageProjectileTrailingVFX class to unmarshal; the case pins the original's member writes and M->U round trip until the import lands");
ORACLE_CASE_PENDING("messages-50A6C0", SetVariableUnmarshal,
    "ours has no CMessageSetVariable::UnmarshalMessage body; the case pins the original until the import lands");
ORACLE_CASE_PENDING("messages-502D60", LoadDialogUnmarshal,
    "ours has no CMessageLoadDialog::UnmarshalMessage body; the case pins the original until the import lands");
ORACLE_CASE_PENDING("messages-4FDD30", ContinueDialogUnmarshal,
    "ours has no CMessageContinueDialog class; the case pins the original's member writes and abort paths until the import lands");
