// iwd2r. Distributed under LICENSE.md.
// Task 21 enables this test: it adds this file and the iwd2r_engine objects to
// iwd2r_tests once the engine links. Until then it is not built, and the
// [.pending] tag keeps it out of a default run once it is.
//
// Fixture: `CNetwork n;` constructs without the chitin object, but the
// null-provider session sequence does not: HostNewSession, AddPlayerToList
// (called by CreatePlayer) and OnCloseSession call the
// g_pChitin->OnMultiplayer* hooks, and the incoming-queue code locks
// g_pChitin->cNetwork.field_F52. The test therefore needs a live g_pChitin
// (the engine's CBaldurChitin); Task 21 supplies it when it wires the engine.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CNetwork.h"

class CChitin;
extern CChitin* g_pChitin;

TEST_CASE("null service provider hosts a single-player session", "[.pending][network]")
{
    REQUIRE(g_pChitin != nullptr);

    CNetwork n;

    REQUIRE(n.EnumerateServiceProviders() == TRUE);
    REQUIRE(n.m_nTotalServiceProviders == 1);
    REQUIRE(n.SelectServiceProvider(CNetwork::SERV_PROV_NULL) == TRUE);
    REQUIRE(n.InitializeConnectionToServiceProvider(TRUE) == TRUE);

    n.m_sSessionNameToMake = "s";
    n.m_bSessionNameToMake = TRUE;
    n.m_sLocalPlayerName = "p";
    n.m_bPlayerNameToMake = TRUE;

    REQUIRE(n.HostNewSession() == TRUE);

    INT err = -1;
    REQUIRE(n.CreatePlayer(err) == TRUE);
    CHECK(err == CNetwork::ERROR_NONE);

    CHECK(n.GetSessionOpen() == TRUE);
    CHECK(n.GetSessionHosting() == TRUE);
    CHECK(n.m_idLocalPlayer == 1);

    CString s;
    n.GetHostPlayerName(s);
    CHECK(s == "p");

    CHECK(n.FindPlayerLocationByID(1, FALSE) == 0);

    BYTE data[4] = { 1, 2, 3, 4 };
    CHECK(n.SendSpecificMessage("p", 0, 1, 1, data, 4) == TRUE);
    CHECK(n.PeekSpecificMessage("p", 1, 1) == FALSE);

    n.CloseSession(TRUE);
    CHECK(n.GetSessionOpen() == FALSE);
    CHECK(n.m_idLocalPlayer == 0);
}
