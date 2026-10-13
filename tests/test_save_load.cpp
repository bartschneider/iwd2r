#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CDimm.h"
#include "CInfButtonArray.h"
#include "CMessage.h"
#include "CNetwork.h"
#include "CVariableHash.h"
#include "CWorldMap.h"
#include "Icewind586B70.h"
#include "ScreenLayout.h"
#include "install_ini_backup.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace {

// The engine in this process, as iwd2r_main builds it, without its threads.
// Built once and leaked: the engine's globals live until the process ends.
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
    if (!platform::ResolveDataDir()) {
        startError = "data folder not found";
        error = startError;
        return false;
    }
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        startError = screen.error;
        error = startError;
        return false;
    }
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale)) {
        startError = "SDL video could not start";
        error = startError;
        return false;
    }
    CBaldurChitin* chitin = new CBaldurChitin();
    if (chitin->field_1932 != 0) {
        startError = "CBaldurChitin refused to start";
        error = startError;
        return false;
    }
    chitin->Init(NULL);
    started = true;
    return true;
}

std::vector<char> ReadAll(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void WriteAll(const std::filesystem::path& path, const std::vector<char>& bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// A fresh directory under the system temp folder, removed when it goes.
class TempDir {
public:
    explicit TempDir(const char* tag)
    {
        std::random_device rd;
        m_path = std::filesystem::temp_directory_path() / (std::string("iwd2r-") + tag + "-" + std::to_string(rd()));
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return m_path; }
    // The directory as CDimm takes it: with a trailing separator.
    CString Dir() const { return CString((m_path.string() + "/").c_str()); }

private:
    std::filesystem::path m_path;
};

CVariable MakeVariable(const char* name, LONG value)
{
    CVariable var;
    var.SetName(CString(name));
    var.m_intValue = value;
    return var;
}

}

// Places a variable in its hash slot as AddKey does, without AddKey's read
// of the game's state (field_1B84 of CInfGame).
static void PlaceVariable(CVariableHash& hash, const char* name, LONG value)
{
    CVariable var = MakeVariable(name, value);
    int index = hash.Hash(CString(name));
    while (hash.m_hashEntries[index].m_name[0] != '\0') {
        index = (index + 1) % hash.m_nTableEntries;
    }
    hash.m_hashEntries[index] = var;
}

TEST_CASE("CVariableHash::Marshal puts each variable in its own slot", "[vars]")
{
    CVariableHash hash(64);
    PlaceVariable(hash, "SAVE_A", 1);
    PlaceVariable(hash, "SAVE_B", 2);
    PlaceVariable(hash, "SAVE_C", 3);

    CVariable* pList = NULL;
    WORD nCount = 0;
    hash.Marshal(&pList, &nCount);
    REQUIRE(nCount == 3);
    REQUIRE(pList != NULL);

    // Every slot is filled, each with a different variable.
    std::vector<std::string> names;
    for (WORD i = 0; i < nCount; i++) {
        names.emplace_back(static_cast<const char*>(pList[i].GetName()));
        CHECK(!names.back().empty());
    }
    CHECK(names[0] != names[1]);
    CHECK(names[1] != names[2]);
    CHECK(names[0] != names[2]);

    // Unmarshal into a new table and find each value again.
    CVariableHash copy(64);
    for (WORD i = 0; i < nCount; i++) {
        PlaceVariable(copy, static_cast<const char*>(pList[i].GetName()), pList[i].m_intValue);
    }
    delete[] pList;

    const char* const kNames[] = { "SAVE_A", "SAVE_B", "SAVE_C" };
    for (LONG i = 0; i < 3; i++) {
        CVariable* pFound = copy.FindKey(CString(kNames[i]));
        REQUIRE(pFound != NULL);
        CHECK(pFound->m_intValue == i + 1);
    }
}

TEST_CASE("CVariableHash::Marshal round trips through AddKey", "[vars][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    InstallIniBackup backup(env);
    std::string error;
    REQUIRE(StartEngine(error));

    CVariableHash hash(64);
    CVariable a = MakeVariable("SAVE_A", 1);
    CVariable b = MakeVariable("SAVE_B", 2);
    CVariable c = MakeVariable("SAVE_C", 3);
    REQUIRE(hash.AddKey(a));
    REQUIRE(hash.AddKey(b));
    REQUIRE(hash.AddKey(c));

    CVariable* pList = NULL;
    WORD nCount = 0;
    hash.Marshal(&pList, &nCount);
    REQUIRE(nCount == 3);

    // Unmarshal the way CInfGame and CGameArea do: AddKey each record.
    CVariableHash copy(64);
    for (WORD i = 0; i < nCount; i++) {
        REQUIRE(copy.AddKey(pList[i]));
    }
    delete[] pList;

    const char* const kNames[] = { "SAVE_A", "SAVE_B", "SAVE_C" };
    for (LONG i = 0; i < 3; i++) {
        CVariable* pFound = copy.FindKey(CString(kNames[i]));
        REQUIRE(pFound != NULL);
        CHECK(pFound->m_intValue == i + 1);
    }
}

TEST_CASE("Icewind586B70::sub_587510 lists the ids of the filled entries", "[icewind]")
{
    Icewind586B70 limits;
    CHECK(limits.sub_587510().empty());

    limits.mEntries[1].field_0 = 11;
    limits.mEntries[1].field_4 = 21;
    limits.mEntries[3].field_0 = 13; // no second id: not counted
    limits.mEntries[4].field_0 = 14;
    limits.mEntries[4].field_4 = 24;
    const std::vector<int> ids = limits.sub_587510();
    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 11);
    CHECK(ids[1] == 14);
}

TEST_CASE("CInfButtonArray keeps the customizable buttons and the weapon set", "[buttons]")
{
    CInfButtonArray* pButtons = new CInfButtonArray();
    INT types[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    pButtons->UpdateCustomizableButtonTypes(types);
    for (int i = 0; i < 9; i++) {
        CHECK(pButtons->m_customizableButtonTypes[i] == i + 1);
    }
    pButtons->UpdateSelectedWeaponSet(3);
    CHECK(pButtons->m_nSelectedWeaponSet == 3);
    delete pButtons;
}

TEST_CASE("CompressDirectory packs a directory that UncompressDirectory restores", "[dimm][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    InstallIniBackup backup(env);
    std::string error;
    REQUIRE(StartEngine(error));

    TempDir source("dimm-a");
    TempDir target("dimm-b");

    std::mt19937 rng(7);
    std::vector<char> area(200000);
    for (char& c : area) {
        c = static_cast<char>(rng() % 16); // compressible, like an area file
    }
    const std::vector<char> small = { 'A', 'R', 'E', 'A', 'V', '9', '.', '1' };
    const std::vector<char> game = { 'G', 'A', 'M', 'E' };
    WriteAll(source.path() / "AR1000.ARE", area);
    WriteAll(source.path() / "AR1001.ARE", small);
    WriteAll(source.path() / "EMPTY.ARE", {});
    WriteAll(source.path() / "ICEWIND2.GAM", game);

    REQUIRE(g_pChitin->cDimm.CompressDirectory(source.Dir()));

    // The packed files are gone; the game file stays and is not packed.
    CHECK(std::filesystem::exists(source.path() / "ICEWIND2.SAV"));
    CHECK(std::filesystem::exists(source.path() / "ICEWIND2.GAM"));
    CHECK_FALSE(std::filesystem::exists(source.path() / "AR1000.ARE"));
    CHECK_FALSE(std::filesystem::exists(source.path() / "AR1001.ARE"));
    CHECK_FALSE(std::filesystem::exists(source.path() / "EMPTY.ARE"));

    std::filesystem::copy_file(source.path() / "ICEWIND2.SAV", target.path() / "ICEWIND2.SAV");
    REQUIRE(g_pChitin->cDimm.UncompressDirectory(target.Dir(), target.Dir()));

    CHECK(ReadAll(target.path() / "AR1000.ARE") == area);
    CHECK(ReadAll(target.path() / "AR1001.ARE") == small);
    CHECK(std::filesystem::exists(target.path() / "EMPTY.ARE"));
    CHECK(ReadAll(target.path() / "EMPTY.ARE").empty());
    CHECK_FALSE(std::filesystem::exists(target.path() / "ICEWIND2.GAM"));
    CHECK_FALSE(std::filesystem::exists(target.path() / "ICEWIND2.SAV"));
}

TEST_CASE("CWorldMap::Marshal writes back the world map it read", "[worldmap][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    InstallIniBackup backup(env);
    std::string error;
    REQUIRE(StartEngine(error));

    std::vector<char> original;
    {
        CWorldMapFile file;
        file.SetResRef(CResRef("WORLDMAP"), TRUE, TRUE);
        BYTE* data = file.GetData();
        REQUIRE(data != NULL);
        original.assign(data, data + file.GetDataSize());
        file.Release();
    }

    // SaveGame passes CInfGame::m_sTempDir; the engine's file writer wants
    // its backslashed form. The data folder is the test's private view.
    const std::filesystem::path written_path = std::filesystem::path(env) / "temp" / "WORLDMAP.WMP";
    std::error_code ec;
    std::filesystem::remove(written_path, ec);

    CWorldMap map;
    map.SetResRef(CResRef("WORLDMAP"));
    REQUIRE(map.m_cHeader.m_nMapCount > 0);
    map.Marshal(CString(".\\temp\\"));

    const std::vector<char> written = ReadAll(written_path);
    std::filesystem::remove(written_path, ec);
    REQUIRE(written.size() == original.size());
    CHECK(written == original);
}

TEST_CASE("NonBlockingWaitForSignal waits for each visible player's signal", "[msg][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    InstallIniBackup backup(env);
    std::string error;
    REQUIRE(StartEngine(error));

    CNetwork& network = g_pChitin->cNetwork;
    CBaldurMessage* pMessage = g_pBaldurChitin->GetBaldurMessage();

    const BOOLEAN wasEstablished = network.m_bConnectionEstablished;
    const int wasHost = network.m_nHostPlayer;
    const BOOLEAN wasVisible[2] = { network.m_pbPlayerVisible[0], network.m_pbPlayerVisible[1] };
    const PLAYER_ID wasId[2] = { network.m_pPlayerID[0], network.m_pPlayerID[1] };

    // Without a session the original answers 0xFF.
    network.m_bConnectionEstablished = FALSE;
    CHECK(pMessage->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_SERVER, CBaldurMessage::SIGNAL_END_MAJOR_EVENT) == 0xFF);

    // Single player: the host is the only visible player.
    network.m_bConnectionEstablished = TRUE;
    network.m_nHostPlayer = 0;
    network.m_pbPlayerVisible[0] = TRUE;
    network.m_pPlayerID[0] = 1;
    network.m_pbPlayerVisible[1] = FALSE;
    network.m_pPlayerID[1] = 0;

    // The server waits for its clients only: there are none.
    CHECK(pMessage->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_SERVER, CBaldurMessage::SIGNAL_END_MAJOR_EVENT) == TRUE);

    // An empty queue, as CBaldurMessage's constructor leaves it.
    for (BYTE i = 0; i < pMessage->m_nSignalQueueSize; i++) {
        pMessage->m_pnSignalFrom[i] = 0xFF;
        pMessage->m_pnSignalType[i] = 0;
        pMessage->m_pnSignalData[i] = 0;
    }
    pMessage->m_nSignalQueueStart = 0;
    pMessage->m_nSignalQueueEnd = 0;

    // A wait for the server needs the host's signal in the queue.
    const BYTE kSignal = 66;
    CHECK(pMessage->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_ALL_CLIENTS, kSignal) == FALSE);
    pMessage->m_pnSignalFrom[pMessage->m_nSignalQueueEnd] = 0;
    pMessage->m_pnSignalType[pMessage->m_nSignalQueueEnd] = CBaldurMessage::SIGNAL_ALL_CLIENTS;
    pMessage->m_pnSignalData[pMessage->m_nSignalQueueEnd] = kSignal;
    pMessage->m_nSignalQueueEnd = static_cast<BYTE>((pMessage->m_nSignalQueueEnd + 1) % pMessage->m_nSignalQueueSize);
    CHECK(pMessage->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_ALL_CLIENTS, kSignal) == TRUE);
    // The signal it waited for is taken off the queue.
    for (BYTE i = 0; i < pMessage->m_nSignalQueueSize; i++) {
        CHECK_FALSE((pMessage->m_pnSignalType[i] == CBaldurMessage::SIGNAL_ALL_CLIENTS && pMessage->m_pnSignalData[i] == kSignal));
    }

    network.m_bConnectionEstablished = wasEstablished;
    network.m_nHostPlayer = wasHost;
    network.m_pbPlayerVisible[0] = wasVisible[0];
    network.m_pbPlayerVisible[1] = wasVisible[1];
    network.m_pPlayerID[0] = wasId[0];
    network.m_pPlayerID[1] = wasId[1];
}
