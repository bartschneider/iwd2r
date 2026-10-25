// The store screen's chat edit box status, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CScreenStore::GetChatEditBoxStatus (0x67E420), __thiscall `ret 8`:
// Get(CString* outText, BOOL* flag). It reads the button-bar panel at
// this+0x5B4 twice: the id at +0x20 for the gate, and then
// CUIPanel::GetControl (0x4D2CA0, which walks the CList at panel+8: the
// control at node+8, its id at +0xA) on that same bar panel. Ours' port
// checks the same member (m_pButtonBar->m_nID) but calls
// m_pMainPanel->GetControl, so each fixture wires the edit into the list
// its side walks: the bar's on the original's, the main's on ours'. With
// control 8 missing, the original runs its assert reporter (0x780C00),
// which suspends threads through g_pChitin at 0x8CF6D8 — engine state a
// case cannot set up, which is why both lists must carry the edit. Get
// mismatches: ours gates on id 3 (src/CScreenStore.cpp:5931), the
// original on 15 (0x67E43F's `cmp [ecx+0x20],0xf`). Get's open path goes
// through the control's GetText (vtable slot 0x6C, sret return the
// caller re-uses as operator='s source); the flag compares the focused
// control pointer at +0x44 against the found control. The edit is a fake
// control with a case vtable: the GetText stubs hand back "chat text"
// (ours' CString on our side, the original's on its side). The offsets
// above are the original's; ours' fixture is built through the named
// members, since ours' compiled layout drifts from the original's
// annotations wherever packing differs.
//
// SetChatEditBoxStatus (0x67E4F0) is not here: its case killed the
// process at the first input (runs/20261025-144803: 0xC0000005, no case
// input recorded) across thirteen launches, while the whole original path
// — the same 15 gate and bar-panel GetControl Get exercises, the
// caller's CString copy, SetText and CUIManager::SetCapture (0x4D4030,
// verified trivial against the fixture) — checks out instruction by
// instruction, so the death sits below what a case can set up.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CScreenStore.h"
#include "CUIPanel.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

using GetFn = void(__fastcall*)(void* self, void* edx, void* outText, int32_t* flag);
constexpr uintptr_t kGet = 0x67E420;

constexpr size_t kScreenWidth = 0x5E0;
constexpr size_t kPanelWidth = 0x28;
constexpr size_t kEditWidth = 0x80;

uintptr_t __fastcall NeutralStub(const void*, void*)
{
    return 0;
}

// These are virtual-call targets: the __thiscall call site keeps this in ECX
// and passes every other argument on the stack (`push sret; mov ecx,this;
// call [vtable+0x6C]`), so a __fastcall(self, edx, arg) stub meets it —
// ECX takes this, EDX absorbs nothing, and the third parameter lands on the
// stack, which __fastcall also pops on return. GetText() returns CString by
// value: the return slot is that stack argument.
void* g_oursVtable[0x20];
void* g_originalVtable[0x20];

// The sret pointer comes back in EAX — the caller passes it straight to
// CString::operator= — so the stubs must return it, not leave EAX to the
// constructor chain's leftovers.
void* __fastcall OursGetText(void*, void*, void* sret)
{
    new (sret) CString("chat text");
    return sret;
}

void* __fastcall OriginalGetText(void*, void*, void* sret)
{
    reinterpret_cast<OriginalCStringFn>(kOriginalCString)(sret, nullptr, "chat text");
    return sret;
}

void InitVtables()
{
    if (g_oursVtable[0] == 0) {
        for (void*& slot : g_oursVtable) {
            slot = reinterpret_cast<void*>(&NeutralStub);
        }
        for (void*& slot : g_originalVtable) {
            slot = reinterpret_cast<void*>(&NeutralStub);
        }
        g_oursVtable[27] = reinterpret_cast<void*>(&OursGetText); // 0x6C
        g_originalVtable[27] = reinterpret_cast<void*>(&OriginalGetText); // 0x6C
    }
}

void PutDword(uint8_t* at, uint32_t value)
{
    std::memcpy(at, &value, 4);
}

// The fake screen for ours: zeroed storage with a placement-new list on
// the main panel, the edit AddTail'd in, the bar gate and the focused
// pointer in place. Ours' real GetControl and GetText run against it.
struct OursFixture {
    uint8_t* screen;
    uint8_t* mainPanel;
    uint8_t* barPanel;
    uint8_t* edit;
};

OursFixture BuildOurs(uint32_t gate, bool focusEdit)
{
    InitVtables();
    OursFixture fixture = {};
    fixture.screen = new uint8_t[sizeof(CScreenStore)]();
    fixture.mainPanel = new uint8_t[sizeof(CUIPanel)]();
    fixture.barPanel = new uint8_t[sizeof(CUIPanel)]();
    fixture.edit = new uint8_t[kEditWidth]();

    // Every ours-side field is set through its member: ours' engine code
    // reads these members at its own compiled offsets, which drift from the
    // original's annotations wherever packing differs.
    auto* screen = reinterpret_cast<CScreenStore*>(fixture.screen);
    auto* mainPanel = reinterpret_cast<CUIPanel*>(fixture.mainPanel);
    auto* barPanel = reinterpret_cast<CUIPanel*>(fixture.barPanel);
    auto* edit = reinterpret_cast<CUIControlBase*>(fixture.edit);
    using ControlList = CTypedPtrList<CPtrList, CUIControlBase*>;
    auto* list = new (&mainPanel->m_lControls) ControlList();
    // The array decays to its address and that pointer becomes the fake
    // vtable pointer. Not a 4-byte memcpy of &g_oursVtable: the optimizer
    // folds that into a copy of the array's first element, NeutralStub, and
    // the virtual calls below dispatch through int3 padding.
    *reinterpret_cast<void***>(fixture.edit) = g_oursVtable;
    edit->m_nID = 8;
    list->AddTail(edit);

    barPanel->m_nID = gate;
    screen->m_pMainPanel = mainPanel;
    screen->m_pButtonBar = barPanel;
    screen->m_cUIManager.m_pFocusedControl = focusEdit ? edit : nullptr;
    return fixture;
}

// The fake screen for the original: original-heap blocks with the
// walk-only list wired by hand into the bar panel, since GetControl never
// allocates and the original never reads the main panel pointer.
struct OriginalFixture {
    uint8_t* screen;
    uint8_t* edit;
};

OriginalFixture BuildOriginal(uint32_t gate, bool focusEdit)
{
    auto* screen = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kScreenWidth));
    std::memset(screen, 0, kScreenWidth);
    auto* bar = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kPanelWidth));
    std::memset(bar, 0, kPanelWidth);
    auto* node = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(0x10));
    std::memset(node, 0, 0x10);
    auto* edit = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kEditWidth));
    std::memset(edit, 0, kEditWidth);

    *reinterpret_cast<void***>(edit) = g_originalVtable;
    PutDword(edit + 0xA, 8);
    PutDword(node + 8, reinterpret_cast<uint32_t>(edit));
    PutDword(bar + 8, reinterpret_cast<uint32_t>(node));
    PutDword(bar + 0x20, gate);
    PutDword(screen + 0x5B4, reinterpret_cast<uint32_t>(bar));
    PutDword(screen + 0x44, focusEdit ? reinterpret_cast<uint32_t>(edit) : 0);
    return { screen, edit };
}

void CheckGet(CaseContext& ctx, uint32_t gate, bool focusEdit)
{
    ctx.SetInputs("Get with the bar panel id " + std::to_string(gate)
        + (focusEdit ? ", the edit focused" : ", nothing focused"));

    const OursFixture fixture = BuildOurs(gate, focusEdit);
    CString oursText;
    BOOL oursFlag = FALSE;
    reinterpret_cast<CScreenStore*>(fixture.screen)->CScreenStore::GetChatEditBoxStatus(oursText, oursFlag);

    uint32_t originalOut = 0;
    int32_t originalFlag = 0;
    ctx.CallOriginal([&] {
        const OriginalFixture fixture = BuildOriginal(gate, focusEdit);
        reinterpret_cast<OriginalCStringFn>(kOriginalCString)(&originalOut, nullptr, "initial");
        reinterpret_cast<GetFn>(kGet)(fixture.screen, nullptr, &originalOut, &originalFlag);
    });

    ctx.CountInputs(1);
    const char* originalText = *reinterpret_cast<const char* const*>(&originalOut);
    oracle::Compare(ctx, std::string(originalText), std::string(static_cast<const char*>(oursText)));
    oracle::Compare(ctx, originalFlag, static_cast<int32_t>(oursFlag));
}

// Get over each gate with the edit focused and not, ours' id 3 against
// the original's 15.
void StoreChatGet(CaseContext& ctx)
{
    for (const bool focusEdit : { true, false }) {
        for (const uint32_t gate : { 3u, 15u, 7u }) {
            CheckGet(ctx, gate, focusEdit);
        }
    }
}

} // namespace

ORACLE_CASE("store_chat-67E420", StoreChatGet);
