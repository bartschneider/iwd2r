#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CScreenCreateChar.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {

// The engine in this process, as iwd2r_main builds it, up to the point
// where CScreenCreateChar::StartCreateChar would make its sprite. Built once
// and leaked: the engine's globals live until the process ends.
bool StartEngine(std::string& error)
{
    static bool started = false;
    static std::string startError;
    static bool tried = false;
    if (tried) {
        error = startError;
        return started;
    }
    tried = true;

#ifdef _WIN32
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
    // The engine writes into its data folder (temp/, cache/, the INI files):
    // it starts on a private view, which lives until the process ends.
    static std::unique_ptr<DataView> view;
    const char* install = std::getenv("IWD2_DATA");
    if (install != nullptr && *install != '\0') {
        const std::string original = install;
        view = std::make_unique<DataView>(original);
#ifdef _WIN32
        _putenv_s("IWD2_DATA", view->Path().c_str());
#else
        setenv("IWD2_DATA", view->Path().c_str(), 1);
#endif
        const bool found = platform::ResolveDataDir();
#ifdef _WIN32
        _putenv_s("IWD2_DATA", original.c_str());
#else
        setenv("IWD2_DATA", original.c_str(), 1);
#endif
        if (!found) {
            startError = "data folder not found";
            return false;
        }
    } else if (!platform::ResolveDataDir()) {
        startError = "data folder not found";
        return false;
    }
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        startError = screen.error;
        return false;
    }
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale)) {
        startError = "SDL video could not start";
        return false;
    }
    CBaldurChitin* chitin = new CBaldurChitin();
    if (chitin->field_1932 != 0) {
        startError = "CBaldurChitin refused to start";
        return false;
    }
    chitin->Init(NULL);
    // CChitin::InitInstance selects the starting engine and the engines'
    // video mode; without its threads, give the Character Generation engine
    // the video mode the sprite's palettes read (CVidPalette::SetType).
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

// The bytes of CHARBASE.CRE through the engine's resource system.
std::vector<BYTE> LoadCharBase()
{
    CCreatureFile file;
    file.SetResRef(CResRef("CHARBASE"), TRUE, TRUE);
    BYTE* data = file.GetData();
    std::vector<BYTE> bytes;
    if (data != NULL) {
        bytes.assign(data, data + file.GetDataSize());
    }
    file.Release();
    return bytes;
}

// A sprite made the way StartCreateChar makes it (0x610540).
CGameSprite* MakeSprite(std::vector<BYTE>& bytes)
{
    return new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
}

// The offsets where two byte strings differ, for a readable failure.
std::string Differences(const std::vector<BYTE>& a, const std::vector<BYTE>& b, size_t base)
{
    std::string out;
    int shown = 0;
    for (size_t i = 0; i < a.size() && i < b.size() && shown < 16; i++) {
        if (a[i] != b[i]) {
            char line[64];
            snprintf(line, sizeof(line), "+0x%zX: %02X != %02X; ", base + i, a[i], b[i]);
            out += line;
            shown++;
        }
    }
    if (a.size() != b.size()) {
        out += "sizes " + std::to_string(a.size()) + " != " + std::to_string(b.size());
    }
    return out;
}

std::vector<BYTE> MarshalSprite(CGameSprite* pSprite)
{
    BYTE* pData = NULL;
    LONG nSize = 0;
    WORD facing = 0;
    pSprite->Marshal(&pData, &nSize, &facing, FALSE, FALSE);
    std::vector<BYTE> bytes;
    if (pData != NULL) {
        bytes.assign(pData, pData + nSize);
        delete[] pData;
    }
    return bytes;
}

}

TEST_CASE("the creature header has the original's size", "[cre]")
{
    // CGameSprite::Unmarshal (0x70D1D2) and Marshal (0x70B5FC) copy 0xDF
    // dwords; a CRE V2.2's offsets block follows at 8 + 0x37C = 900.
    STATIC_REQUIRE(sizeof(CCreatureFileHeader) == 0x37C);
    STATIC_REQUIRE(offsetof(CCreatureFileHeader, field_2E2) == 0x2E2);
    STATIC_REQUIRE(offsetof(CCreatureFileHeader, field_2E8) == 0x2E8);
    STATIC_REQUIRE(offsetof(CCreatureFileHeader, field_2F8) == 0x2F8);
    STATIC_REQUIRE(offsetof(CCreatureFileHeader, m_fadeSpeed) == 0x2FA);
    STATIC_REQUIRE(offsetof(CCreatureFileHeader, field_2FC) == 0x2FC);
    STATIC_REQUIRE(offsetof(CCreatureFileHeader, field_2FF) == 0x2FF);
}

TEST_CASE("CHARBASE.CRE survives Unmarshal and Marshal", "[cre][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::vector<BYTE> original = LoadCharBase();
    REQUIRE(original.size() > 900);
    REQUIRE(memcmp(original.data(), "CRE V2.2", 8) == 0);

    CGameSprite* pSprite = MakeSprite(original);
    REQUIRE(pSprite != NULL);

    CCreatureFileHeader* pBStats = pSprite->GetBaseStats();

    // The header's ability bytes, file offsets 0x266..0x26B, in the order
    // CDerivedStats::Reload (0x4440F0) reads them: STR, INT, WIS, DEX, CON, CHR.
    CHECK(pBStats->m_STRBase == original[0x266]);
    CHECK(pBStats->m_INTBase == original[0x267]);
    CHECK(pBStats->m_WISBase == original[0x268]);
    CHECK(pBStats->m_DEXBase == original[0x269]);
    CHECK(pBStats->m_CONBase == original[0x26A]);
    CHECK(pBStats->m_CHRBase == original[0x26B]);
    CHECK(pSprite->m_derivedStats.m_nSTR == original[0x266]);
    CHECK(pSprite->m_derivedStats.m_nINT == original[0x267]);
    CHECK(pSprite->m_derivedStats.m_nWIS == original[0x268]);
    CHECK(pSprite->m_derivedStats.m_nDEX == original[0x269]);
    CHECK(pSprite->m_derivedStats.m_nCON == original[0x26A]);
    CHECK(pSprite->m_derivedStats.m_nCHR == original[0x26B]);

    // Project 2 read feats[1] of a fresh character from the original.
    CHECK(pBStats->m_feats[1] == 0x03F01FC0);

    // Hit points, file offsets 0x24 and 0x26.
    SHORT hp;
    WORD maxHp;
    memcpy(&hp, &original[0x24], sizeof(hp));
    memcpy(&maxHp, &original[0x26], sizeof(maxHp));
    // In memory they carry the Constitution bonus, which Marshal (0x70B2F0)
    // takes off again.
    CHECK(hp == 10);
    CHECK(maxHp == 10);
    CHECK(pSprite->m_nHPCONBonusTotalOld == -1);
    CHECK(pBStats->m_hitPoints == 9);
    CHECK(pBStats->m_maxHitPointsBase == 9);
    CHECK(pBStats->m_hitPoints == hp + pSprite->m_nHPCONBonusTotalOld);
    CHECK(pBStats->m_maxHitPointsBase == maxHp + pSprite->m_nHPCONBonusTotalOld);

    std::vector<BYTE> first = MarshalSprite(pSprite);
    REQUIRE(first.size() > 900);
    CHECK(memcmp(first.data(), "CRE V2.2", 8) == 0);
    // The header round-trips byte for byte, apart from what Unmarshal
    // (0x70CF90) and Marshal (0x70B2F0) change on purpose: Marshal writes
    // effect version 1 (+0x2B), and Unmarshal turns a favored enemy of 0
    // into R_NO_RACE (+0x253..+0x25A).
    std::vector<BYTE> header(first.begin() + 8, first.begin() + 8 + sizeof(CCreatureFileHeader));
    std::vector<BYTE> expected(original.begin() + 8, original.begin() + 8 + sizeof(CCreatureFileHeader));
    expected[0x2B] = 1;
    for (size_t nIndex = 0; nIndex < MAX_HATED_RACES; nIndex++) {
        if (expected[0x253 + nIndex] == 0) {
            expected[0x253 + nIndex] = CAIObjectType::R_NO_RACE;
        }
    }
    // Unmarshal fills a base attack bonus (+0x48) and number of attacks
    // (+0x49) of 0 from CRuleTables::sub_546B60 (0x546B60): a level 1
    // fighter has +1 and one attack.
    CHECK(original[8 + 0x48] == 0);
    CHECK(original[8 + 0x49] == 0);
    CHECK(pBStats->m_attackBase == 1);
    CHECK(pBStats->m_numberOfAttacksBase == 1);
    expected[0x48] = pBStats->m_attackBase;
    expected[0x49] = pBStats->m_numberOfAttacksBase;
    CHECK(Differences(header, expected, 8) == "");

    CGameSprite* pSecond = MakeSprite(first);
    REQUIRE(pSecond != NULL);
    std::vector<BYTE> second = MarshalSprite(pSecond);

    // The second sprite has its own object id, which Unmarshal stores as the
    // AI type's instance (offsets block +0xC).
    REQUIRE(second.size() >= 900 + 0x10);
    REQUIRE(first.size() >= 900 + 0x10);
    memset(&second[900 + 0xC], 0, 4);
    memset(&first[900 + 0xC], 0, 4);
    CHECK(Differences(second, first, 0) == "");
}

TEST_CASE("a CRE whose equipped item is missing loads with that slot empty", "[cre][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::vector<BYTE> original = LoadCharBase();
    REQUIRE(original.size() > 900);

    // Append one item, ZZZZZZZZ, and equip it as the helmet. The offsets
    // block starts at 900; the item list's offset is at +0x292, its count at
    // +0x296, the equipment block's offset at +0x28E (CCreatureFileOffsets).
    std::vector<BYTE> bytes = original;
    DWORD itemOffset;
    DWORD itemCount;
    DWORD equipmentOffset;
    memcpy(&equipmentOffset, &bytes[900 + 0x28E], 4);
    memcpy(&itemOffset, &bytes[900 + 0x292], 4);
    memcpy(&itemCount, &bytes[900 + 0x296], 4);
    REQUIRE(itemCount == 0);

    CCreatureFileItem item;
    memcpy(item.m_itemId, "ZZZZZZZZ", RESREF_SIZE);
    const DWORD newOffset = static_cast<DWORD>(bytes.size());
    bytes.insert(bytes.end(), reinterpret_cast<BYTE*>(&item), reinterpret_cast<BYTE*>(&item) + sizeof(item));
    const DWORD one = 1;
    memcpy(&bytes[900 + 0x292], &newOffset, 4);
    memcpy(&bytes[900 + 0x296], &one, 4);
    const WORD slot = 0;
    memcpy(&bytes[equipmentOffset + 0x0], &slot, sizeof(slot));

    CGameSprite* pSprite = MakeSprite(bytes);
    REQUIRE(pSprite != NULL);
    CHECK(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_HELMET] == NULL);
}

TEST_CASE("a CRE's equipped item survives Unmarshal and Marshal", "[cre][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::vector<BYTE> original = LoadCharBase();
    REQUIRE(original.size() > 900);

    // Append one item, 00AMUL01 (an amulet with no equipped effects), and
    // equip it in the first misc slot (CCreatureFileEquipment +0x24).
    std::vector<BYTE> bytes = original;
    DWORD equipmentOffset;
    DWORD itemCount;
    memcpy(&equipmentOffset, &bytes[900 + 0x28E], 4);
    memcpy(&itemCount, &bytes[900 + 0x296], 4);
    REQUIRE(itemCount == 0);

    CCreatureFileItem item;
    memcpy(item.m_itemId, "00AMUL01", RESREF_SIZE);
    item.m_usageCount[0] = 1;
    const DWORD newOffset = static_cast<DWORD>(bytes.size());
    bytes.insert(bytes.end(), reinterpret_cast<BYTE*>(&item), reinterpret_cast<BYTE*>(&item) + sizeof(item));
    const DWORD one = 1;
    memcpy(&bytes[900 + 0x292], &newOffset, 4);
    memcpy(&bytes[900 + 0x296], &one, 4);
    const size_t miscOffset = offsetof(CCreatureFileEquipment, m_miscItem);
    const WORD slot = 0;
    memcpy(&bytes[equipmentOffset + miscOffset], &slot, sizeof(slot));

    CGameSprite* pSprite = MakeSprite(bytes);
    REQUIRE(pSprite != NULL);
    CItem* pItem = pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC];
    REQUIRE(pItem != NULL);
    CHECK(pItem->GetResRef() == CResRef("00AMUL01"));

    std::vector<BYTE> first = MarshalSprite(pSprite);
    REQUIRE(first.size() > 900);
    DWORD outOffset;
    DWORD outCount;
    DWORD outEquipment;
    memcpy(&outOffset, &first[900 + 0x292], 4);
    memcpy(&outCount, &first[900 + 0x296], 4);
    memcpy(&outEquipment, &first[900 + 0x28E], 4);
    REQUIRE(outCount == 1);
    REQUIRE(outOffset + sizeof(CCreatureFileItem) <= first.size());
    CHECK(memcmp(&first[outOffset], "00AMUL01", RESREF_SIZE) == 0);
    WORD outSlot;
    memcpy(&outSlot, &first[outEquipment + miscOffset], sizeof(outSlot));
    CHECK(outSlot == 0);

    // A second pass gives the same bytes, apart from the object id.
    CGameSprite* pSecond = MakeSprite(first);
    REQUIRE(pSecond != NULL);
    std::vector<BYTE> second = MarshalSprite(pSecond);
    REQUIRE(second.size() >= 900 + 0x10);
    memset(&second[900 + 0xC], 0, 4);
    memset(&first[900 + 0xC], 0, 4);
    CHECK(Differences(second, first, 0) == "");
}

TEST_CASE("a fresh character's effect list runs as Character Generation runs it", "[cre][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::vector<BYTE> original = LoadCharBase();
    REQUIRE(original.size() > 900);

    // CScreenCreateChar::StartCreateChar (0x610540) makes the sprite, then
    // sets field_562C and calls ProcessEffectList (0x72DE60), which reloads
    // the derived stats through HandleEffects (0x734550).
    CGameSprite* pSprite = MakeSprite(original);
    REQUIRE(pSprite != NULL);
    const SHORT hp = pSprite->GetBaseStats()->m_hitPoints;
    pSprite->field_562C = 1;
    pSprite->ProcessEffectList();

    CHECK(pSprite->field_562C == 0);
    CHECK(pSprite->GetBaseStats()->m_hitPoints == hp);
    CHECK(pSprite->m_derivedStats.m_nSTR == original[0x266]);
    for (int nSkill = 0; nSkill < 16; nSkill++) {
        CHECK(pSprite->m_derivedStats.m_nSkills[nSkill] >= 0);
    }
}

TEST_CASE("Marshal without all items skips a type 0x3A item and keeps the slot numbers", "[cre][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    // 00GBGA01 is of type 0x3A in the vanilla data; 00AMUL01 (an amulet)
    // is not.
    const CString sFound("00GBGA01");
    const CString sOther("00AMUL01");

    // CItem(CResRef, ...) (0x4E7E90) sets the item's resource and
    // CResItem::Parse (0x4016F0) accepts the 'ITM ' header.
    {
        CItem probe(CResRef(sOther), 0, 0, 0, 0, 0);
        REQUIRE(probe.GetResRef() == CResRef(sOther));
    }

    std::vector<BYTE> original = LoadCharBase();
    REQUIRE(original.size() > 900);
    CGameSprite* pSprite = MakeSprite(original);
    REQUIRE(pSprite != NULL);

    // A type-0x3A item in the first misc slot and an ordinary one in the
    // second, so the second's number depends on the first being skipped.
    pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC] = new CItem(CResRef(sFound), 0, 0, 0, 0, 0);
    pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC + 1] = new CItem(CResRef(sOther), 0, 0, 0, 0, 0);
    REQUIRE(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC]->GetItemType() == 0x3A);
    REQUIRE(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC + 1]->GetItemType() != 0x3A);

    CCreatureFileEquipment equipment;
    CCreatureFileItem* pItems = NULL;
    DWORD nItems = 0;
    pSprite->m_equipment.Marshal(&equipment, &pItems, &nItems, FALSE);

    // The fist is not counted and the 0x3A item is left out: one item, at 0.
    CHECK(nItems == 1);
    CHECK(equipment.m_miscItem[0] == 0xFFFF);
    CHECK(equipment.m_miscItem[1] == 0);
    REQUIRE(pItems != NULL);
    CHECK(CResRef(pItems[0].m_itemId) == CResRef(sOther));
    delete[] pItems;

    // With all items both are written, in slot order.
    pItems = NULL;
    pSprite->m_equipment.Marshal(&equipment, &pItems, &nItems, TRUE);
    CHECK(nItems == 2);
    CHECK(equipment.m_miscItem[0] == 0);
    CHECK(equipment.m_miscItem[1] == 1);
    delete[] pItems;
}

namespace {

// The bytes of a creature's CRE through the engine's resource system.
std::vector<BYTE> LoadCre(const char* resRef)
{
    CCreatureFile file;
    file.SetResRef(CResRef(resRef), TRUE, TRUE);
    BYTE* data = file.GetData();
    std::vector<BYTE> bytes;
    if (data != NULL) {
        bytes.assign(data, data + file.GetDataSize());
    }
    file.Release();
    return bytes;
}

}

TEST_CASE("creatures 10GUTHE and 12OGR construct from their CRE", "[cre][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    for (const char* resRef : { "10GUTHE", "12OGR" }) {
        INFO(resRef);
        std::vector<BYTE> bytes = LoadCre(resRef);
        REQUIRE(bytes.size() > 900);
        CGameSprite* pSprite = MakeSprite(bytes);
        REQUIRE(pSprite != NULL);
    }
}

// The constructor (0x6EF990) zeroes 0x3428-0x3DFA before Unmarshal re-equips
// a creature's weapon set (sub_726810), which reads the quick weapons'
// ammunition slots (field_3D3A). 10GOBAR, AR1000's goblin archer, has a
// shortbow and arrows: built over memory of zeroes or of 0xFF bytes, it
// selects the arrows (the first quiver slot, 11) for its first weapon
// button either way.
TEST_CASE("a goblin archer equips its bow the same over any heap contents", "[cre][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::vector<BYTE> bytes = LoadCre("10GOBAR");
    REQUIRE(bytes.size() > 900);

    // Never freed: the sprites stay in the game's object array, as
    // MakeSprite's do.
    CGameSprite* sprites[2];
    const int fills[2] = { 0x00, 0xFF };
    for (int n = 0; n < 2; n++) {
        void* memory = ::operator new(sizeof(CGameSprite));
        std::memset(memory, fills[n], sizeof(CGameSprite));
        sprites[n] = new (memory) CGameSprite(bytes.data(),
            static_cast<LONG>(bytes.size()),
            0,
            0,
            static_cast<DWORD>(-1),
            0,
            0,
            INT_MAX,
            CPoint(-1, -1),
            0);
    }

    for (int n = 0; n < 2; n++) {
        INFO("fill " << fills[n]);
        CHECK(sprites[n]->field_3D3A[0] == 11);
        for (int button = 1; button < 8; button++) {
            CHECK(sprites[n]->field_3D3A[button] == 0);
        }
        CHECK(sprites[n]->m_nLastSong == 0);
        CHECK(sprites[n]->m_equipment.m_selectedWeapon == 11);
    }
    CHECK(sprites[0]->m_equipment.m_selectedWeaponAbility == sprites[1]->m_equipment.m_selectedWeaponAbility);
}

// A manual sweep, hidden from the default run: constructs every creature
// named in IWD2R_CRE_LIST (separated by spaces, commas or newlines),
// printing each name before it is built, so a crash names its creature.
// Run it one creature per process under the memory cap (rule M3-3:
// systemd-run --user --scope -p MemoryMax=... -p MemorySwapMax=0): the
// unported effects of some creatures loop or assert while they load.
TEST_CASE("every creature in IWD2R_CRE_LIST constructs from its CRE", "[.][cre-sweep][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    const char* list = std::getenv("IWD2R_CRE_LIST");
    if (env == nullptr || *env == '\0' || list == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA or IWD2R_CRE_LIST unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::string names(list);
    for (char& c : names) {
        if (c == ',' || c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
    }
    size_t at = 0;
    while (at < names.size()) {
        size_t end = names.find(' ', at);
        if (end == std::string::npos) {
            end = names.size();
        }
        const std::string resRef = names.substr(at, end - at);
        at = end + 1;
        if (resRef.empty()) {
            continue;
        }
        fprintf(stderr, "cre-sweep: constructing %s\n", resRef.c_str());
        fflush(stderr);
        std::vector<BYTE> bytes = LoadCre(resRef.c_str());
        CHECK(bytes.size() > 900);
        if (bytes.size() > 900) {
            CGameSprite* pSprite = MakeSprite(bytes);
            CHECK(pSprite != NULL);
        }
        fprintf(stderr, "cre-sweep: built %s\n", resRef.c_str());
        fflush(stderr);
    }
}

// CItem's constructors from a CRE item record (0x4E7B60) and from another
// item (0x4E7900), and its destructor (0x4E8180). 00AROW01 stacks to 40
// and has lore 0; 00AROW03 stacks to 40 and has lore 4.
TEST_CASE("CItem from a CRE record clamps stack counts and marks identification; a copy does neither", "[cre][item][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CCreatureFileItem record;
    memcpy(record.m_itemId, "00arow01", RESREF_SIZE);
    record.m_wear = 3;
    record.m_dynamicFlags = 0x10;

    // Lore 0: flag 0x1 (identified), so no 0x8. A stackable item's counts
    // are at least 1.
    CItem unknown(record);
    CHECK(unknown.GetResRef() == CResRef("00AROW01"));
    CHECK(unknown.m_useCount1 == 1);
    CHECK(unknown.m_useCount2 == 1);
    CHECK(unknown.m_useCount3 == 1);
    CHECK(unknown.m_wear == 3);
    CHECK(unknown.m_flags == (0x10 | 0x1));
    CHECK(unknown.m_nAbilities == unknown.GetAbilityCount());

    // Lore 4 and flag 0x1 clear: 0x8 is set.
    memcpy(record.m_itemId, "00AROW03", RESREF_SIZE);
    record.m_usageCount[0] = 7;
    record.m_dynamicFlags = 0;
    CItem arrows(record);
    CHECK(arrows.m_useCount1 == 7);
    CHECK(arrows.m_flags == 0x8);

    // The copy keeps the counts and flags as they are: no clamp, no 0x8.
    arrows.m_useCount2 = 0;
    arrows.m_flags = 0;
    arrows.m_numSounds = 2;
    CItem copy(arrows);
    CHECK(copy.GetResRef() == CResRef("00AROW03"));
    CHECK(copy.m_useCount1 == 7);
    CHECK(copy.m_useCount2 == 0);
    CHECK(copy.m_useCount3 == 1);
    CHECK(copy.m_flags == 0);
    CHECK(copy.m_numSounds == 2);
    CHECK(copy.m_wear == 3);

    // A copy of an item with lore 0 marks it identified.
    unknown.m_flags = 0;
    CItem copyUnknown(unknown);
    CHECK(copyUnknown.m_flags == 0x1);

    // The destructor resets the counts, the ability count and the wear.
    alignas(CItem) unsigned char storage[sizeof(CItem)];
    CItem* pItem = new (storage) CItem(record);
    pItem->m_useCount1 = 9;
    pItem->m_wear = 5;
    pItem->~CItem();
    CItem* pDead = reinterpret_cast<CItem*>(storage);
    CHECK(pDead->m_useCount1 == 1);
    CHECK(pDead->m_useCount2 == 1);
    CHECK(pDead->m_useCount3 == 1);
    CHECK(pDead->m_nAbilities == 0);
    CHECK(pDead->m_wear == 0);
}

// CItem::Equip (0x4E8860) adds an item's equipped effects to the sprite's
// equipped list; Unequip (0x4E8DF0) takes them out again. 00CIBBN1 is a
// creature ring with one equipped effect (timing 2).
TEST_CASE("CItem::Equip adds a ring's equipped effect and Unequip removes it", "[cre][item][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    std::vector<BYTE> bytes = LoadCharBase();
    REQUIRE(bytes.size() > 900);
    CGameSprite* pSprite = MakeSprite(bytes);
    REQUIRE(pSprite != NULL);

    CItem ring(CResRef("00CIBBN1"), 0, 0, 0, 0, 0);
    REQUIRE(ring.pRes != NULL);
    const INT_PTR before = pSprite->GetEquipedEffectList()->GetCount();

    ring.Equip(pSprite, CGameSpriteEquipment::SLOT_RING_LEFT, FALSE);
    CHECK(pSprite->GetEquipedEffectList()->GetCount() == before + 1);
    CHECK(pSprite->field_7544 == 0);

    // Animation only: no effects.
    ring.Equip(pSprite, CGameSpriteEquipment::SLOT_RING_LEFT, TRUE);
    CHECK(pSprite->GetEquipedEffectList()->GetCount() == before + 1);

    ring.Unequip(pSprite, CGameSpriteEquipment::SLOT_RING_LEFT, FALSE, FALSE);
    CHECK(pSprite->GetEquipedEffectList()->GetCount() == before);

    // An empty item does nothing.
    CItem none;
    none.Equip(pSprite, CGameSpriteEquipment::SLOT_RING_LEFT, FALSE);
    none.Unequip(pSprite, CGameSpriteEquipment::SLOT_RING_LEFT, FALSE, FALSE);
    CHECK(pSprite->GetEquipedEffectList()->GetCount() == before);
}
