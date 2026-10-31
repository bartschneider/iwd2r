// CGameSpy's heartbeat and socket lifecycle (0x4D0980, 0x4D1EC0, 0x4D2060,
// 0x4D2310), linked against the engine.
//
// The platform's winsock calls are inert by default (WSAStartup fails), so the
// tests install fakes through iwd2r_SetGameSpySockets: a startup that can
// succeed, handles that record their calls, a sendto that keeps the datagram,
// and a WSAAsyncGetHostByName whose completion the test's thread signals
// through the 0x26 flag (what CChitin::WndProc's 0x402 arm sets in the game).
//
// Fixture: a bare CChitin subclass supplies g_pChitin (the four thread bodies
// are pure virtual and never run) and its cNetwork (the reply section the
// functions lock, and the host flag). The class's own fields the constructor
// leaves alone (the two sockets and the master address) are set explicitly.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CGameSpy.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

extern CChitin* g_pChitin;

namespace {

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }

    void GetGameSpyGameName(CString& sGameSpyName) override { sGameSpyName = "IWDTEST"; }
    WORD GetMultiplayerGameSpyPort() override { return m_nGameSpyPort; }

    WORD m_nGameSpyPort = 8000;
};

class ChitinScope {
public:
    ChitinScope()
        : m_previous(g_pChitin)
    {
        g_pChitin = &m_chitin;
    }
    ~ChitinScope() { g_pChitin = m_previous; }

    ChitinScope(const ChitinScope&) = delete;
    ChitinScope& operator=(const ChitinScope&) = delete;

    TestChitin& Chitin() { return m_chitin; }

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

// The fake winsock calls and what they recorded.
struct SocketLog {
    bool bStartupOk = true;
    bool bSocketFails = false;
    WORD nWSAVersion = 2;
    int nStartupCalls = 0;
    int nCleanupCalls = 0;
    int nSocketCalls = 0;
    SOCKET nNextSocket = 100;
    std::vector<SOCKET> opened;
    std::vector<SOCKET> closed;
    int nBoundPort = -1;
    SOCKET nBoundSocket = 0;
    std::string sSent;
    int nSentFlags = -1;
    unsigned short nSentPort = 0;
    unsigned long nSentAddress = 0;
    int nAsyncCalls = 0;
    int nAsyncCancelCalls = 0;
    // Filled by WSAAsyncGetHostByName; the resolved HOSTENT the port reads.
    unsigned long nResolvedAddress = 0x01020304;
    void* pAddrList[2] = { &nResolvedAddress, nullptr };
    CGameSpy* pSpyToResolve = nullptr;
};

SocketLog g_log;

int FakeStartup(WORD, WSADATA* data)
{
    g_log.nStartupCalls++;
    std::memset(data, 0, sizeof(*data));
    data->wVersion = g_log.bStartupOk ? g_log.nWSAVersion : 0;
    return g_log.bStartupOk ? 0 : SOCKET_ERROR;
}

int FakeCleanup()
{
    g_log.nCleanupCalls++;
    return 0;
}

SOCKET FakeSocket(int af, int type, int protocol)
{
    g_log.nSocketCalls++;
    if (g_log.bSocketFails || af != AF_INET || type != SOCK_DGRAM || protocol != IPPROTO_UDP) {
        return INVALID_SOCKET;
    }
    SOCKET handle = g_log.nNextSocket++;
    g_log.opened.push_back(handle);
    return handle;
}

int FakeIoctl(SOCKET, long command, unsigned long* argument)
{
    if (command != FIONBIO || argument == nullptr || *argument != 1) {
        return SOCKET_ERROR;
    }
    return 0;
}

int FakeBind(SOCKET handle, const struct sockaddr_in* address, int length)
{
    if (length != static_cast<int>(sizeof(*address)) || address->sin_family != AF_INET) {
        return SOCKET_ERROR;
    }
    g_log.nBoundSocket = handle;
    g_log.nBoundPort = address->sin_port;
    return 0;
}

int FakeSendto(SOCKET, const char* buffer, int length, int flags, const struct sockaddr_in* to,
    int toLength)
{
    if (length < 0 || to == nullptr || toLength != static_cast<int>(sizeof(*to))) {
        return SOCKET_ERROR;
    }
    g_log.sSent.assign(buffer, static_cast<size_t>(length));
    g_log.nSentFlags = flags;
    g_log.nSentPort = to->sin_port;
    g_log.nSentAddress = to->sin_addr.s_addr;
    return length;
}

int FakeRecvfrom(SOCKET, char*, int, int, struct sockaddr_in*, int*)
{
    return SOCKET_ERROR;
}

int FakeClosesocket(SOCKET handle)
{
    g_log.closed.push_back(handle);
    return 0;
}

HANDLE FakeAsyncGetHostByName(HWND, unsigned message, const char* name, char* buffer, int length)
{
    g_log.nAsyncCalls++;
    REQUIRE(message == 0x402);
    REQUIRE(name != nullptr);
    REQUIRE(std::string(name) == "master.gamespy.com");
    REQUIRE(buffer != nullptr);
    REQUIRE(length == 0x400);

    // A synthetic HOSTENT: h_addr_list at +0x0C.
    std::memset(buffer, 0, static_cast<size_t>(length));
    *reinterpret_cast<void**>(buffer + 0x0C) = g_log.pAddrList;

    // The window procedure sets the flag when the request completes; a thread
    // stands in for it, while the caller sleeps in SleepEx's 300 ms slice.
    CGameSpy* pSpy = g_log.pSpyToResolve;
    if (pSpy != nullptr) {
        std::thread([pSpy] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            pSpy->m_bHostResolved = TRUE;
        }).detach();
    }
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x1234));
}

int FakeCancelAsync(HANDLE)
{
    g_log.nAsyncCancelCalls++;
    return 0;
}

const IWD2R_GameSpySockets kFakeSockets = {
    FakeStartup,
    FakeCleanup,
    FakeSocket,
    FakeIoctl,
    FakeBind,
    FakeSendto,
    FakeRecvfrom,
    FakeClosesocket,
    FakeAsyncGetHostByName,
    FakeCancelAsync,
};

// Installs the fake table for the scope, and clears the log.
class SocketScope {
public:
    SocketScope()
    {
        g_log = SocketLog();
        g_log.opened.clear();
        g_log.closed.clear();
        // The temporary's pAddrList points at its own member; keep the live
        // one.
        g_log.pAddrList[0] = &g_log.nResolvedAddress;
        iwd2r_SetGameSpySockets(&kFakeSockets);
    }
    ~SocketScope() { iwd2r_SetGameSpySockets(nullptr); }

    SocketScope(const SocketScope&) = delete;
    SocketScope& operator=(const SocketScope&) = delete;
};

// A CGameSpy with the fields its constructor does not set (the two sockets
// and the master address) cleared, so a test never reads indeterminate state.
void InitSpy(CGameSpy& spy)
{
    spy.m_hSendSocket = 0;
    spy.m_hQuerySocket = 0;
    spy.m_nMasterAddress = 0;
}

}

TEST_CASE("gamespy: with the inert winsock the heartbeat never starts", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    CGameSpy spy;
    InitSpy(spy);

    CHECK(spy.sub_4D2060() == FALSE);
    CHECK(spy.m_bHeartbeatActive == FALSE);
    CHECK(spy.m_bWSAStarted == FALSE);
}

TEST_CASE("gamespy: the heartbeat opens both sockets, binds the query port and closes them", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    chitin.Chitin().m_nGameSpyPort = 0x1F40;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    g_log.pSpyToResolve = &spy;

    REQUIRE(spy.sub_4D2060() == TRUE);
    CHECK(spy.m_bHeartbeatActive == TRUE);
    CHECK(spy.m_bWSAStarted == TRUE);
    CHECK(spy.m_nMasterAddress == 0x01020304);
    CHECK(spy.m_nPort == 0x1F40);
    CHECK(spy.m_nRetries == 0);
    CHECK(spy.m_dwLastHeartbeat != 0);
    CHECK(g_log.nStartupCalls == 1);
    REQUIRE(g_log.opened.size() == 2);
    CHECK(spy.m_hSendSocket == g_log.opened[0]);
    CHECK(spy.m_hQuerySocket == g_log.opened[1]);
    CHECK(g_log.nBoundSocket == spy.m_hQuerySocket);
    CHECK(g_log.nBoundPort == htons(0x1F40));
    CHECK(g_log.nCleanupCalls == 0);

    CHECK(spy.sub_4D2310() == TRUE);
    REQUIRE(g_log.closed.size() == 2);
    CHECK(g_log.closed[0] == spy.m_hSendSocket);
    CHECK(g_log.closed[1] == spy.m_hQuerySocket);
    CHECK(g_log.nCleanupCalls == 1);
    CHECK(spy.m_bWSAStarted == FALSE);

    // The original clears +0x00 (winsock started) but not +0x01 (active), so
    // a second close closes the same stale handles again and still returns
    // TRUE; winsock is not cleaned up twice.
    g_log.closed.clear();
    CHECK(spy.sub_4D2310() == TRUE);
    CHECK(g_log.closed.size() == 2);
    CHECK(g_log.nCleanupCalls == 1);
    CHECK(spy.m_bWSAStarted == FALSE);
}

TEST_CASE("gamespy: a non-host does not start the heartbeat", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = FALSE;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    g_log.pSpyToResolve = &spy;

    CHECK(spy.sub_4D2060() == FALSE);
    CHECK(spy.m_bHeartbeatActive == FALSE);
    CHECK(g_log.opened.empty());
}

TEST_CASE("gamespy: an unset GameSpy port falls back to 0x5BE", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    chitin.Chitin().m_nGameSpyPort = 0;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    g_log.pSpyToResolve = &spy;

    REQUIRE(spy.sub_4D2060() == TRUE);
    CHECK(spy.m_nPort == 0x5BE);
    CHECK(g_log.nBoundPort == htons(0x5BE));
}

TEST_CASE("gamespy: a failed host lookup leaves the heartbeat off", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    SocketScope sockets;
    g_log.bStartupOk = false;
    CGameSpy spy;
    InitSpy(spy);

    CHECK(spy.sub_4D0980() == FALSE);
    CHECK(spy.m_bWSAStarted == FALSE);
    CHECK(g_log.nStartupCalls == 1);
    CHECK(g_log.nCleanupCalls == 1);
    CHECK(g_log.nAsyncCalls == 0);
}

TEST_CASE("gamespy: the heartbeat is the master protocol's query string", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_bHeartbeatActive = TRUE;
    spy.m_nPort = 8000;
    spy.m_nMasterAddress = 0x7F000001;

    REQUIRE(spy.sub_4D1EC0(FALSE) == TRUE);
    CHECK(g_log.sSent == "\\heartbeat\\8000\\gamename\\IWDTEST\\\n");
    CHECK(g_log.nSentFlags == 0);
    CHECK(g_log.nSentPort == htons(0x6CFC));
    CHECK(g_log.nSentAddress == 0x7F000001);

    REQUIRE(spy.sub_4D1EC0(TRUE) == TRUE);
    CHECK(g_log.sSent == "\\heartbeat\\8000\\gamename\\IWDTEST\\statechanged\\\n");

    // A1 is compared against 1 exactly: any other value takes the plain form.
    REQUIRE(spy.sub_4D1EC0(2) == TRUE);
    CHECK(g_log.sSent == "\\heartbeat\\8000\\gamename\\IWDTEST\\\n");
}

TEST_CASE("gamespy: a heartbeat that is off or not from the host is not sent", "[gamespy]")
{
    ChitinScope chitin;
    chitin.Chitin().cNetwork.m_bIsHost = TRUE;
    SocketScope sockets;
    CGameSpy spy;
    InitSpy(spy);
    spy.m_nPort = 8000;

    // Not active.
    CHECK(spy.sub_4D1EC0(FALSE) == FALSE);
    CHECK(g_log.sSent.empty());

    // Active, but this is not the host.
    spy.m_bHeartbeatActive = TRUE;
    chitin.Chitin().cNetwork.m_bIsHost = FALSE;
    CHECK(spy.sub_4D1EC0(FALSE) == FALSE);
    CHECK(g_log.sSent.empty());
}
