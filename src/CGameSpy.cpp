#include "CGameSpy.h"

#include "CBaldurChitin.h"
#include "CChitin.h"

// #binary-identical
// 0x4D0930
CGameSpy::CGameSpy()
{
    m_dwLastHeartbeat = 0;
    m_nPort = 0;
    m_nResponseLength = 0;
    m_pResponse = NULL;
    m_bHeartbeatActive = 0;
    m_nQueryCounter = 0;
    m_nQueryId = 0;
    m_nRetries = 0;
    m_bWSAStarted = 0;
    m_bHostResolved = 0;
}

// #binary-identical
// 0x4D0960
CGameSpy::~CGameSpy()
{
    if (m_bWSAStarted == 1) {
        WSACleanup();
    }
    m_bWSAStarted = 0;
    m_bHeartbeatActive = 0;
}

// Starts winsock if it is not running and resolves master.gamespy.com through
// WSAAsyncGetHostByName: the request posts the 0x402 message to the game
// window, whose handler sets +0x26 (CChitin::WndProc), and this waits for it
// in up to ten 300 ms slices before cancelling the request. On success the
// first address of the returned HOSTENT is kept at +0x06.
//
// NOTE: The original returns from its two failure paths without leaving the
// reply section (#design-flaw). The port leaves it: the paths are unreachable
// when winsock starts, and on this engine the inert WSAStartup always takes
// the first one, so reproducing the leak would strand the section.
//
// 0x4D0980
BOOLEAN CGameSpy::sub_4D0980()
{
    CString sLocation("master.gamespy.com");

    EnterCriticalSection(&g_pChitin->cNetwork.field_F6A);

    if (m_bWSAStarted == FALSE) {
        WSADATA wsaData;
        WSAStartup(2, &wsaData);

        if (wsaData.wVersion == 2) {
            m_bWSAStarted = TRUE;
        } else {
            WSACleanup();
            m_bWSAStarted = FALSE;
        }

        if (m_bWSAStarted == FALSE) {
            LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
            return FALSE;
        }
    }

    HWND hWnd = NULL;
    if (g_pChitin->GetWnd() != NULL) {
        hWnd = g_pChitin->GetWnd()->GetSafeHwnd();
    }

    BYTE hostBuffer[0x400];
    HANDLE hTask = WSAAsyncGetHostByName(hWnd, 0x402, sLocation,
        reinterpret_cast<char*>(hostBuffer), 0x400);
    m_bHostResolved = FALSE;

    BYTE* pHostent = NULL;
    if (hTask == NULL) {
        m_bWSAStarted = FALSE;
        WSACleanup();
    } else {
        BYTE nTries = 0;
        do {
            nTries++;
            SleepEx(300, FALSE);
            if (m_bHostResolved != FALSE) {
                break;
            }
        } while (nTries < 10);

        if (m_bHostResolved == FALSE) {
            WSACancelAsyncRequest(hTask);
            m_bWSAStarted = FALSE;
            WSACleanup();
        } else {
            pHostent = hostBuffer;
        }
    }

    if (m_bWSAStarted == TRUE) {
        if (pHostent != NULL) {
            // HOSTENT: h_addr_list at +0x0C, its first entry the address in
            // network order.
            BYTE** ppAddrList = *reinterpret_cast<BYTE***>(pHostent + 0xC);
            m_nMasterAddress = *reinterpret_cast<unsigned long*>(ppAddrList[0]);
        } else {
            m_bWSAStarted = FALSE;
            WSACleanup();
        }
    }

    LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
    return m_bWSAStarted;
}

// Sends the master server one heartbeat: the query string is
// `\heartbeat\<port>\gamename\<name>\` and, when a1 is 1, also the
// `statechanged\` key, each ending in a newline. The message goes to the
// master address at +0x06, port htons(0x6CFC), from the send socket at +0x0A.
//
// NOTE: The original returns from its two allocation failures without leaving
// the reply section (#design-flaw); the port leaves it, for the reason in
// sub_4D0980's NOTE (the path is a heap failure, not the normal one).
//
// 0x4D1EC0
BOOLEAN CGameSpy::sub_4D1EC0(BOOL a1)
{
    CString sGameSpyName;

    if (m_bHeartbeatActive == FALSE || g_pChitin->cNetwork.m_bIsHost == FALSE) {
        return FALSE;
    }

    EnterCriticalSection(&g_pChitin->cNetwork.field_F6A);

    char* pQuery = new char[0x50];
    if (pQuery == NULL) {
        LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
        return FALSE;
    }

    g_pChitin->GetGameSpyGameName(sGameSpyName);
    if (a1 == 1) {
        sprintf(pQuery, "\\heartbeat\\%d\\gamename\\%s\\statechanged\\\n", m_nPort,
            static_cast<const char*>(sGameSpyName));
    } else {
        sprintf(pQuery, "\\heartbeat\\%d\\gamename\\%s\\\n", m_nPort,
            static_cast<const char*>(sGameSpyName));
    }

    // The original allocates the exact string length and copies that many
    // bytes: the datagram has no terminator and sendto gets the terminator's
    // index.
    INT nLength = static_cast<INT>(strlen(pQuery));
    char* pMessage = new char[nLength];
    if (pMessage == NULL) {
        delete[] pQuery;
        LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
        return FALSE;
    }
    memcpy(pMessage, pQuery, nLength);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0x6CFC);
    addr.sin_addr.s_addr = m_nMasterAddress;
    sendto(m_hSendSocket, pMessage, nLength, 0, &addr, sizeof(addr));

    delete[] pMessage;
    delete[] pQuery;

    LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
    return TRUE;
}

// Opens the heartbeat: resolves the master (sub_4D0980), then makes the send
// socket (+0x0A) and the query socket (+0x0E) non-blocking and binds the
// latter to the GameSpy port from the chitin (0x5BE when the INI has none).
// Any failure closes what was opened and leaves +0x01 clear. A successful
// open sets +0x12 to the port, +0x28 (the retry counter) to zero, and on
// failure calls WSACleanup.
//
// 0x4D2060
BOOLEAN CGameSpy::sub_4D2060()
{
    if (sub_4D0980() == FALSE) {
        m_bHeartbeatActive = FALSE;
        return FALSE;
    }

    EnterCriticalSection(&g_pChitin->cNetwork.field_F6A);

    // The first heartbeat is 30 s later.
    m_dwLastHeartbeat = GetTickCount() - 30000;

    WORD nPort = g_pChitin->GetMultiplayerGameSpyPort();
    if (nPort == 0) {
        nPort = 0x5BE;
    }

    m_bHeartbeatActive = TRUE;
    if (m_bWSAStarted == FALSE || g_pChitin->cNetwork.m_bIsHost == FALSE) {
        m_bHeartbeatActive = FALSE;
    }

    if (m_bHeartbeatActive == TRUE) {
        BOOLEAN bOpened = FALSE;
        if (m_bWSAStarted != FALSE) {
            SOCKET hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (hSocket != INVALID_SOCKET) {
                unsigned long bNonBlocking = 1;
                if (ioctlsocket(hSocket, FIONBIO, &bNonBlocking) == 0) {
                    m_hSendSocket = hSocket;
                    bOpened = TRUE;
                } else {
                    closesocket(hSocket);
                }
            }
        }
        if (bOpened == FALSE) {
            m_bHeartbeatActive = FALSE;
        }
    }

    if (m_bHeartbeatActive == TRUE) {
        BOOLEAN bOpened = FALSE;
        if (m_bWSAStarted != FALSE) {
            SOCKET hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (hSocket != INVALID_SOCKET) {
                unsigned long bNonBlocking = 1;
                if (ioctlsocket(hSocket, FIONBIO, &bNonBlocking) == 0) {
                    m_hQuerySocket = hSocket;
                    bOpened = TRUE;
                } else {
                    closesocket(hSocket);
                }
            }
        }
        if (bOpened == FALSE) {
            m_bHeartbeatActive = FALSE;
        }
    }

    if (m_bHeartbeatActive == TRUE) {
        BOOLEAN bBound = FALSE;
        if (m_bWSAStarted != FALSE) {
            sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(nPort);
            addr.sin_addr.s_addr = 0;
            if (bind(m_hQuerySocket, &addr, sizeof(addr)) == 0) {
                bBound = TRUE;
            } else {
                closesocket(m_hQuerySocket);
            }
        }
        if (bBound == FALSE) {
            closesocket(m_hSendSocket);
            m_bHeartbeatActive = FALSE;
        }
    }

    m_nPort = nPort;
    m_nRetries = 0;

    if (m_bHeartbeatActive == FALSE) {
        // #design-flaw: the original sets +0x00 to 1 around WSACleanup even
        // though winsock never started; kept as the bytes have it.
        m_bWSAStarted = TRUE;
        WSACleanup();
        m_bWSAStarted = FALSE;
    }

    LeaveCriticalSection(&g_pChitin->cNetwork.field_F6A);
    return m_bHeartbeatActive;
}

// 0x4D2210
BOOLEAN CGameSpy::sub_4D2210()
{
    // TODO: Incomplete.

    return FALSE;
}

// Closes the heartbeat sockets and winsock when a heartbeat was running;
// returns FALSE when none was.
//
// 0x4D2310
BOOLEAN CGameSpy::sub_4D2310()
{
    if (m_bHeartbeatActive == FALSE) {
        return FALSE;
    }

    closesocket(m_hSendSocket);
    closesocket(m_hQuerySocket);

    if (m_bWSAStarted == TRUE) {
        WSACleanup();
        m_bWSAStarted = FALSE;
    }

    return TRUE;
}
