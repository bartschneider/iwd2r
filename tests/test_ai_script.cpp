#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAICondition.h"
#include "CAIResponse.h"
#include "CAITrigger.h"

// The compiled-script readers on a hand-written condition-response block in
// the BCS text layout: CAICondition::Read (0x404290) gives each trigger its
// own text, CAIResponse::Read (0x40E100) takes the weight in front of the
// first action, CAIResponseSet starts with a weight total of 0 (0x40FA30),
// and CAIResponse::InListEnd (0x40E0C0) tests only the last action.

namespace {

// Two triggers: Global("CHAPTER","GLOBAL",-1) and EntirePartyOnMap().
const char* kCondition = "TR\n"
                         "16399 -1 0 0 0 \"GLOBALCHAPTER\" \"\" OB\n"
                         "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                         "TR\n"
                         "TR\n"
                         "16593 0 0 0 0 \"\" \"\" OB\n"
                         "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                         "TR\n";

// The text of one response (between its RE lines): weight 70, then
// Continue() and EndCutSceneMode().
const char* kResponse = "70AC\n"
                        "36OB\n"
                        "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                        "OB\n"
                        "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                        "OB\n"
                        "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                        "0 0 0 0 0\"\" \"\" AC\n"
                        "AC\n"
                        "122OB\n"
                        "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                        "OB\n"
                        "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                        "OB\n"
                        "0 0 0 0 0 0 0 0 0 0 0 0 0 [-1.-1.-1.-1] \"\" 0 0 OB\n"
                        "0 0 0 0 0\"\" \"\" AC\n";

} // namespace

TEST_CASE("script condition reads each trigger from its own text", "[script][ai]")
{
    CAICondition condition;
    condition.Read(CString(kCondition));

    REQUIRE(condition.m_triggerList.GetCount() == 2);

    POSITION pos = condition.m_triggerList.GetHeadPosition();
    CAITrigger* pGlobal = condition.m_triggerList.GetNext(pos);
    CHECK(pGlobal->m_triggerID == 0x400F);
    CHECK(pGlobal->m_specificID == -1);
    CHECK(pGlobal->m_string1 == CString("GLOBALCHAPTER"));

    CAITrigger* pPartyOnMap = condition.m_triggerList.GetNext(pos);
    CHECK(pPartyOnMap->m_triggerID == 0x40D1);
    CHECK(pPartyOnMap->m_specificID == 0);
}

TEST_CASE("script response reads its weight and actions", "[script][ai]")
{
    CAIResponse response;
    response.Read(CString(kResponse));

    CHECK(response.m_weight == 70);
    REQUIRE(response.m_actionList.GetCount() == 2);
    CHECK(response.m_actionList.GetHead()->m_actionID == CAIAction::CONTINUE);
    CHECK(response.m_actionList.GetTail()->m_actionID == 122);

    // Continue() is in the list but not at its end.
    CHECK(response.InList(CAIAction::CONTINUE));
    CHECK_FALSE(response.InListEnd(CAIAction::CONTINUE));
    CHECK(response.InListEnd(122));
}

TEST_CASE("script response set totals the weights from 0", "[script][ai]")
{
    CAIResponseSet responseSet;
    CHECK(responseSet.m_weightTotal == 0);

    CAIResponse response;
    response.Read(CString(kResponse));
    responseSet.Add(response);
    responseSet.Add(response);

    CHECK(responseSet.m_weightTotal == 140);
    REQUIRE(responseSet.m_responseList.GetCount() == 2);

    for (int round = 0; round < 50; round++) {
        CAIResponse* pChosen = responseSet.Choose();
        REQUIRE(pChosen != NULL);
        CHECK((pChosen->m_responseNum == 1 || pChosen->m_responseNum == 2));
    }
}
