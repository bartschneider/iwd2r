#include "CSwitchCDStatus.h"

#include "CBaldurChitin.h"
#include "CVideo.h"

// The message layout, in pixels from the screen's top left: .data constants
// the original keeps for this class alone (only 0x54D2xx-0x54E1xx read them).
// The names are guessed from where Update draws with them. #guess
static int s_nPanelX = 280; // 0x8AE4A0
static int s_nPanelY = 228; // 0x8AE4A4
static int s_nTitleY = 276; // 0x8AE4A8
static int s_nLine1Y = 291; // 0x8AE4AC
static int s_nLine2Y = 306; // 0x8AE4B0
static int s_nLine3Y = 321; // 0x8AE4B4
static int s_nLine4Y = 10; // 0x8AE4B8

// The wrapped message block, set by the compiler-generated initialisers at
// 0x54D240 (position) and 0x54D260 (size). #guess
static int s_nTextX = 0x23; // 0x8E0320
static int s_nTextY = 0x221; // 0x8E0324
static int s_nTextWidth = 0x2FD; // 0x8E0330
static int s_nTextHeight = 0x30; // 0x8E0334

// 0x54D280
CSwitchCDStatus::CSwitchCDStatus()
{
    field_0 = 0;
    field_4 = 0;
    m_bActivateEngine = FALSE;
}

// 0x54D360
CSwitchCDStatus::~CSwitchCDStatus()
{
    if (field_4 != 0) {
        field_2D8.Release();
        field_440.Release();
        // The original releases the fonts' CResCell directly (0x77F820);
        // CVidCell::Release(BOOLEAN&) hides the helper's no-argument one.
        field_4F4.CResHelper<CResCell, 1000>::Release();
        field_9F4.CResHelper<CResCell, 1000>::Release();
        field_4 = 0;
    }
}

// 0x54D460
void CSwitchCDStatus::Init()
{
    int x;
    int y;

    if (g_pBaldurChitin->field_4A28 == 0) {
        x = (CVideo::SCREENWIDTH - 800) / 2;
        y = (CVideo::SCREENHEIGHT - 600) / 2;
    } else {
        s_nPanelX *= 2;
        s_nPanelY *= 2;
        s_nTitleY *= 2;
        s_nLine1Y *= 2;
        s_nLine2Y *= 2;
        s_nLine3Y *= 2;
        s_nLine4Y *= 2;
        s_nTextX *= 2;
        s_nTextY *= 2;
        s_nTextWidth *= 2;
        s_nTextHeight *= 2;
        x = (CVideo::SCREENWIDTH >> 1) - 800;
        y = (CVideo::SCREENHEIGHT >> 1) - 600;
    }

    s_nPanelX += x;
    s_nPanelY += y;
    s_nTitleY += y;
    s_nLine1Y += y;
    s_nLine2Y += y;
    s_nLine3Y += y;
    s_nTextX += x;
    s_nTextY += y;
}

// 0x54D5A0
void CSwitchCDStatus::Update(BOOLEAN a1, BOOLEAN a2, BYTE a3, const CString& a4, BOOLEAN a5, BOOLEAN a6, BOOLEAN a7)
{
    // TODO: Incomplete.
}
