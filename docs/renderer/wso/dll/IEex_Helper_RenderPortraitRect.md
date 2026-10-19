# IEex_Helper_RenderPortraitRect

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10033ce0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10033ce0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_10033ce0(int param_1,float *param_2,int *param_3,undefined4 param_4,int param_5,int param_6
                 ,undefined4 param_7,int param_8)

{
  ushort *puVar1;
  LONG *pLVar2;
  int *piVar3;
  char cVar4;
  bool bVar5;
  code *pcVar6;
  UINT UVar7;
  short sVar8;
  ushort uVar9;
  uint uVar10;
  int iVar11;
  BOOL BVar12;
  int iVar13;
  undefined1 *puVar14;
  int *piVar15;
  float *pfVar16;
  undefined4 unaff_EDI;
  int iVar17;
  float fVar18;
  float fVar19;
  float fVar20;
  double dVar21;
  float fVar22;
  undefined4 uVar23;
  undefined1 **ppuVar24;
  undefined1 *puVar25;
  float fVar26;
  undefined1 *puVar27;
  undefined4 uVar28;
  float fVar29;
  code *pcStack_988;
  undefined1 *puStack_984;
  float *local_980;
  undefined4 local_97c;
  char local_975;
  float local_974;
  code *local_970;
  code *pcStack_96c;
  float fStack_968;
  undefined1 auStack_964 [8];
  undefined4 **ppuStack_95c;
  LONG LStack_958;
  uint uStack_954;
  short sStack_950;
  int iStack_94c;
  int local_948;
  byte bStack_941;
  LONG *local_940;
  undefined1 local_93c [16];
  undefined4 **ppuStack_92c;
  LONG *pLStack_928;
  LONG *pLStack_924;
  undefined1 auStack_918 [12];
  RECT RStack_90c;
  undefined4 *local_8fc;
  undefined4 *local_8f8;
  undefined4 uStack_8f4;
  undefined1 auStack_8f0 [16];
  undefined1 *puStack_8e0;
  undefined4 **ppuStack_8dc;
  LONG *pLStack_8d8;
  undefined1 auStack_8d4 [12];
  undefined1 auStack_8c8 [8];
  undefined1 *local_8c0;
  RECT local_8bc;
  undefined4 **ppuStack_8ac;
  undefined1 auStack_8a0 [12];
  float *pfStack_894;
  int iStack_890;
  undefined4 *puStack_88c;
  undefined4 *puStack_888;
  float *apfStack_884 [4];
  undefined1 auStack_874 [16];
  float **ppfStack_864;
  tagRECT tStack_85c;
  undefined1 local_84c;
  undefined1 local_84b;
  undefined1 local_84a;
  undefined1 local_849;
  undefined1 local_848 [12];
  undefined4 **local_83c;
  LONG *local_838;
  LONG *local_834;
  undefined4 *local_830;
  undefined4 **local_82c;
  code *pcStack_780;
  uint local_14;

  local_14 = DAT_1008f040 ^ (uint)&pcStack_988;
  local_948 = param_1;
  local_93c._8_4_ = param_2;
  local_97c = param_3;
  if (DAT_104df536 != '\0') {
    param_8 = 1;
  }
  local_8c0 = (undefined1 *)*param_2;
  local_93c._0_4_ = *param_3;
  local_8bc.left = (LONG)param_2[1];
  local_8bc.top = (LONG)(local_8c0 + local_93c._0_4_);
  local_93c._4_4_ = param_3[1];
  local_8bc.right = local_93c._4_4_ + local_8bc.left;
  local_974 = _DAT_008cf6dc;
  local_970 = *(code **)((int)_DAT_008cf6dc + 0x1c54);
  if ((*(int *)(_DAT_008cf6d8 + 0x96e) == 0) ||
     (*(int *)(_DAT_008cf6d8 + 0x104c) == *(int *)(param_1 + 0x62))) {
    local_980 = (float *)0x1;
  }
  else {
    local_980 = (float *)0x0;
  }
  if ((*(int *)((int)local_970 + 0x43d8) != 0) && (local_980 == (float *)0x0)) {
    return;
  }
  RStack_90c.right = param_1 + 0x53ea;
  local_8fc = (undefined4 *)0x47a5c0;
  pLVar2 = (LONG *)(param_1 + 0x5d0);
  RStack_90c.bottom = param_1 + 0x548a;
  local_848._0_4_ = (LONG *)0x4f504f4e;
  local_975 = *(char *)pLVar2;
  local_848._4_4_ = (float *)0x4d535452;
  if ((_DAT_008e706c != (char *)0x0) && (*_DAT_008e706c != '\0')) {
    uVar10 = 0;
    do {
      if (_DAT_008e706c[uVar10] == '\0') goto LAB_10033f33;
      local_848[uVar10] = _DAT_008e706c[uVar10];
      uVar10 = uVar10 + 1;
    } while ((int)uVar10 < 8);
  }
LAB_10033e0d:
  local_8f8 = (undefined4 *)local_848;
  local_848._8_4_ = (int)&uStack_8f4 + 3;
  uStack_8f4 = (float *)CONCAT13((int)local_93c._0_4_ < (int)local_93c._4_4_,(undefined3)uStack_8f4)
  ;
  local_83c = &local_8fc;
  local_838 = &RStack_90c.bottom;
  local_834 = &RStack_90c.right;
  local_830 = &param_8;
  local_82c = &local_8f8;
  local_940 = pLVar2;
  if (((int)local_93c._0_4_ < (int)local_93c._4_4_) && (local_975 != '\0')) {
    uVar10 = 0;
    do {
      if (*(char *)((int)pLVar2 + uVar10) == '\0') break;
      uVar10 = uVar10 + 1;
    } while ((int)uVar10 < 8);
    if (((1 < uVar10) && (*(char *)((uVar10 - 2) + (int)pLVar2) == '_')) &&
       ((cVar4 = *(char *)((uVar10 - 1) + (int)pLVar2), cVar4 == 'S' || (cVar4 == 's')))) {
      if (uVar10 < 3) {
        tStack_85c.bottom._0_3_ = (uint3)*(ushort *)pLVar2;
LAB_10033f54:
        tStack_85c.bottom = (LONG)(uint3)tStack_85c.bottom;
LAB_10033f5c:
        local_84c = 0;
LAB_10033f64:
        local_84b = 0;
LAB_10033f6c:
        local_84a = 0;
LAB_10033f74:
        local_849 = 0;
      }
      else {
        tStack_85c.bottom._0_3_ = CONCAT12(*(undefined1 *)(param_1 + 0x5d2),*(ushort *)pLVar2);
        if (uVar10 < 4) goto LAB_10033f54;
        tStack_85c.bottom = CONCAT13(*(undefined1 *)(param_1 + 0x5d3),(uint3)tStack_85c.bottom);
        if (uVar10 < 5) goto LAB_10033f5c;
        local_84c = *(undefined1 *)(param_1 + 0x5d4);
        if (uVar10 < 6) goto LAB_10033f64;
        local_84b = *(undefined1 *)(param_1 + 0x5d5);
        if (uVar10 < 7) goto LAB_10033f6c;
        local_84a = *(undefined1 *)(param_1 + 0x5d6);
        if (uVar10 < 8) goto LAB_10033f74;
        local_849 = *(undefined1 *)(param_1 + 0x5d7);
      }
      local_940 = &tStack_85c.bottom;
      *(undefined1 *)((int)&tStack_85c.right + uVar10 + 3) = 0x42;
    }
  }
  (*(code *)0x47a5c0)(local_940,1);
  *(int *)(RStack_90c.right + 0xb6) = param_8;
  if ((local_940 != pLVar2) && (*(int *)(RStack_90c.right + 0xa4) == 0)) {
    (*(code *)0x47a5c0)(pLVar2,1);
    *(int *)(RStack_90c.right + 0xb6) = param_8;
  }
  if ((((local_975 == '\0') || (*(int *)(RStack_90c.right + 0xa4) == 0)) &&
      (FUN_10035370(), *(int *)(RStack_90c.right + 0xa4) == 0)) ||
     ((sVar8 = (*(code *)0x7b2e90)(), sVar8 != 0x18 && (sVar8 != 8)))) {
    FUN_10035370();
  }
  iStack_94c = (param_8 != 0) + 1;
  puStack_984 = (undefined1 *)(*local_97c + iStack_94c * -4);
  pcStack_96c = (code *)(local_97c[1] + iStack_94c * -4);
  auStack_8f0._0_4_ = *(undefined4 *)local_93c._8_4_;
  auStack_8f0._4_4_ = *(float *)(local_93c._8_4_ + 4);
  auStack_8f0._8_4_ = (undefined1 *)(auStack_8f0._0_4_ + iStack_94c * 2) + (int)puStack_984;
    /* ... [middle of function elided for length; full listing in runs/dll-decomp/FUN_10033ce0.c] ... */
        local_8bc.top = local_848._0_4_;
        local_8bc.right = local_848._4_4_;
        local_8bc.bottom = local_848._8_4_;
        ppuStack_8ac = local_83c;
        (**(code **)(*piVar3 + 8))(0,uStack_954,pcStack_988,&local_8bc.top,0,0,0,0xffffffff);
        iStack_94c = iStack_94c + (int)pcStack_96c;
        if ((int)puStack_888 - (int)pcStack_96c < iStack_94c) {
          local_980 = (float *)((int)local_980 - (int)local_97c);
          iStack_94c = iStack_890;
        }
      }
    } while (piVar15 != (int *)0x0);
  }
  iVar17 = local_948;
  if ((0 < (short)local_93c._4_2_) && (DAT_008f950c != '\0')) {
    (*(code *)0x792ca0)();
    (*(code *)0x793170)("NUMFONT",param_8,1);
    (*(code *)0x7ae290)(0);
    local_970 = pcStack_780;
    if ((pcStack_780 != (code *)0x0) && (iVar13 = (*(code *)0x77f4e0)(), iVar13 != 0)) {
      LStack_958 = 0xff;
      if (sStack_950 != 0) {
        iVar13 = 0;
        do {
          if ((int)(&DAT_100882e8)[iVar13] <= ((short)iStack_94c * 100) / (int)sStack_950) {
            LStack_958 = *(LONG *)(&UNK_100872d8 + iVar13 * 4);
            break;
          }
          iVar13 = iVar13 + 1;
        } while (iVar13 < 4);
      }
      if ((uStack_954 & 0x8000000) == 0) {
        wsprintfA((LPSTR)&tStack_85c.bottom,"%d/%d",(int)(short)iStack_94c,(int)sStack_950);
      }
      else {
        lstrcpyA((LPSTR)&tStack_85c.bottom,"-/-");
        LStack_958 = 0xff;
      }
      iStack_890 = 999;
      puStack_88c = (undefined4 *)lstrlenA((LPCSTR)&tStack_85c.bottom);
      puStack_888 = (undefined4 *)0x1f;
      lstrcpynA((LPSTR)apfStack_884,(LPCSTR)&tStack_85c.bottom,0x20);
      ppfStack_864 = apfStack_884;
      (*(code *)0x7930f0)(LStack_958,0,1);
      sVar8 = (*(code *)0x793050)(0);
      (*(code *)0x793720)(auStack_874,local_8bc.bottom,(int)sVar8 + *(int *)(uStack_954 + 4),
                          &puStack_8e0,0);
      (*(code *)0x77f820)();
    }
    (*(code *)0x792d10)();
  }
  if (*(int *)(iVar17 + 0x7248) != 0) {
    (*(code *)0x7ae290)();
    (*(code *)0x7adff0)(0);
    BVar12 = IntersectRect((LPRECT)local_848,(RECT *)(auStack_8a0 + 8),(RECT *)auStack_8c8);
    if (BVar12 == 0) {
      SetRectEmpty((LPRECT)local_848);
    }
    local_8bc.top = local_848._0_4_;
    local_8bc.right = local_848._4_4_;
    local_8bc.bottom = local_848._8_4_;
    ppuStack_8ac = local_83c;
    (**(code **)(*piVar3 + 8))
              (0,iStack_890 - (int)local_974,(undefined1 *)((int)pfStack_894 + (int)puStack_984),
               &local_8bc.top,0,0,0,0xffffffff);
  }
  return;
LAB_10033f33:
  do {
    if (7 < uVar10) {
      FUN_10058959();
      pcVar6 = (code *)swi(3);
      (*pcVar6)();
      return;
    }
    local_848[uVar10] = 0;
    uVar10 = uVar10 + 1;
  } while ((int)uVar10 < 8);
  goto LAB_10033e0d;
}
```

## Notes

### The gui-misc group: seventeen helpers, two install shapes

This is the residual group: the GUI helpers that are neither the hud
registry writers nor the ui-input / keys-screen machinery (those have
their own committed notes and their own shared sections). What ties
these seventeen together is how they are ENTERED, not what they do --
they fall into two disjoint install shapes (decompiled from the fork's
`IEexHelper.fork.dll`):

1. **Lua C exports** (cdecl, bare `ret`): AskResolution,
   DecrementButtonDoRender, GetAttackItemFields, GetUseItemFields,
   RenderButtonIcon, SetPanelMosaicResref. Lua calls these directly
   (`IEex_Helper_<Name>(...)`), the first argument is always the
   `lua_State*`, arguments come off the Lua stack via
   `lua_tointeger` / `lua_tolstring` / `lua_tonumber`, results go
   back with `lua_pushinteger` (the count is the return value), and
   registration is the fork's ExposeFunctions table -- the same
   mechanism as every other `IEex_Helper_*` export.
2. **Engine-hook forwards** (__stdcall, `ret <n>`): the other eleven.
   A Lua-installed engine stub pushes one or two registers
   (`51` = push ecx, `56`/`!push(esi)` = push esi, `57`/`!push(edi)`
   = push edi) and calls the helper with the engine call site's own
   stack arguments below it; the helper gates on DLL-side state,
   optionally replays or replaces the engine work, and its `ret`
   cleans exactly the stub's pushes plus the wrapped call's own
   stack args. Registration for these is the init-time name sweep
   (FUN_1000e2a0), not ExposeFunctions.

#### The engine-address plumbing (one sweep, two exceptions)

Every engine pointer in this group is a `DAT_1009xxxx` BSS slot filled
at DLL init through the loader's PUBLIC address-list mechanism --
Bubb13/IEexLoader, IEexDLL/IEexLua.asm (IEex_Init /
IEex_AddressList); the fork-side lookup is the
`attemptFillPointer()` loop pinned at 0x1000aa2e-0x1000aa79: `[ebp-0x14]`
= key name, `ecx = [ebp+0x2c]` = binding context, `mov eax,[ecx]` /
`lea edx,[ebp-0x14]` / `push edx` / `call [eax+8]` (vtable+8 name
lookup); success stores the engine address into the slot, a miss logs
`"[!][IEexHelper.dll] attemptFillPointer() - Binding pattern [%s]
missing; using this binding will crash the game"` (string at
0x10062248) through the logger at 0x1000e1b0 and leaves the slot
null. The sweep is ~137 named slots; the ones this group reads:

| Slot | Bound name / role |
|---|---|
| DAT_100944b8 | g_pChitin (+0x3c4 current panel id, +0x394 lock, +0x91c 3D/HD-active flag) |
| DAT_10094384 | g_pBaldurChitin (+0x1c54 CInfGame, +0x1c88 expected panel id, +0x190e flags byte, +0x1c64 chargen object, +0x4a28 byte) |
| DAT_10094414 | CVidMosaic::Render (self-registered by the DLL's init at 0x10009f20-0x10009f34 -- the one slot NOT filled by the Lua-side sweep) |
| DAT_10094324 | CUIControlBase::TimerAsynchronousUpdate (control passed on the stack -- byte-confirmed) |
| DAT_1009436c / DAT_100944d0 | CGameObjectArray::GetShare / UndoShare |
| DAT_10094454 | CGameSprite::GetIdleSequence |
| DAT_1009433c / DAT_100944a0 | CItem::Demand / DecrementDemands |
| DAT_10094254 | CItem::GetAbilityNum |
| DAT_100944c4 | CVidCell::RenderIcon |
| DAT_10094268 | CVidCell::Destruct |
| DAT_10094244 / DAT_10094444 | CVidCell::SequenceSet / FrameSet |
| DAT_10094428 | CVidCell::GetCurrentFrameSize |
| DAT_10094474 / DAT_10094394 | CVidCell::GetNumberSequences / GetSequenceLength |
| DAT_10094184 | CVidCell::ConstructFromResRef (forwarded by FUN_1000db20) |
| DAT_100942d0 | CInfToolTip::FormatTooltipPrefix |
| DAT_100943a4 / DAT_1009424c | CResourceManager::GetResObject / CRes::DecrementRequests |
| DAT_10094448 / DAT_10094484 | CSingleLock::Construct / Destruct |
| DAT_100942f4 / DAT_10094158 / DAT_1009437c | CString::Format / Destruct / SetFromChars |
| DAT_10094190 | bound "CString::SetFromCString", used assign/append-shaped (oddity, see SetToolTipOverride) |
| DAT_100942bc | AssertionFailed |
| DAT_100941b0 | CTlkTable::Fetch |
| DAT_10094404 | bound "DirectDrawCreate" -- AskResolution passes it as the GUI dialog's first argument (odd, documented honestly in that note) |
| DAT_10094304 | "CGameSprite::GetLauncher" (key string 0x100620ac) |
| DAT_100941b4 / DAT_100941fc | CInfGame::GetSpellLevelMinusOneForClassAndKit / GetIntModMaxAllowedSpellLevel |
| DAT_10094318 | g_resolution_x (zoom/marker groups also read it) |
| DAT_100942f8 | a global CString slot in the tooltip append path (registered; name not resolved) |

Two exceptions to the sweep: DAT_10094414 above, and the
RenderPortraitRect helper, which additionally hard-links engine
STATICS directly (displacements below the image base, the tile-atlas
group's `_DAT_008cf6d8` class): `_DAT_008cf6dc` = g_pBaldurChitin
global, `_DAT_008cf6d8` = g_pChitin global (read at +0x96e, +0x104c,
+0x91c), `_DAT_008e706c` = a char* font-name override copied over the
"NOPORTSM" default, `_DAT_008ba31c` / `_DAT_008ba31e` = SCREENWIDTH /
SCREENHEIGHT, `_DAT_008f94b8` / `_DAT_008f94bc` = cell-strip stride
multipliers, `DAT_008f950c` / `DAT_008f950d` = NUMFONT gate bytes. A
grep over the whole decompile set confirms those statics appear ONLY
in FUN_10033ce0 -- every other helper in this group goes through the
slots.

#### GL usage

Exactly one helper in this group issues GL: RenderPortraitRect, via
its own six-slot table resolved like the markers/zoom groups'
(resolver identity per the render3d-group note's 16-proc pattern):
DAT_104df4e4 = glEnable, DAT_103deffc = glColor4f, DAT_104df220 =
glBegin, DAT_104df4f0 = glVertex2f, DAT_104df3fc = glEnd,
DAT_104df024 = glDisable. It emits an immediate-mode GL_QUADS rect
and LEAKS state (GL_BLEND left enabled, GL_TEXTURE_2D left disabled)
-- whatever draws next must set its own state, exactly the render3d
group's convention. The other sixteen helpers issue no GL at all.

#### The refonte gate and the panel-id set

The "Modern HUD" (PoE/BG2EE-style) layout is gated Lua-side on
`IEex_PortraitGridEnabled`: declared false at IEex_Gui_State.lua:1615,
WeiDU-MANAGED -- the core ships false and the "Modern HUD" component
(DESIGNATED 103) flips it true by REPLACE_TEXTUALLY (same pattern as
IEEX_HD_UI); the installer's `IEex_InstallPortraitGrid` vetoes it
back to false at GAME LOAD when the runtime requirements (OpenGL
renderer, GUIW10) are unmet. Because of that post-panel-build veto,
`IEEX_PORTRAIT_FRAMES_AVAILABLE = IEex_PortraitGridEnabled` is
captured ONCE at load (State:1630) rather than read live -- the
capture also dodges a two-thread hazard: IEex_OptionRowVisible runs
on BOTH threads (rows built on Sync/CHU init, IEex_InitOptionButtons
on Async). The live re-test at State:5181-5183 is
`local refonte = IEex_PortraitGridEnabled and chuResref == "GUIW10"
and chitin ~= 0x0 and IEex_ReadDword(chitin + 0x91C) ~= 0x0` with
`local chitin = IEex_ReadDword(0x8CF6D8)` -- the same +0x91c flag the
DLL reads through DAT_100944b8.

Panel ids used across the group (Lua-pinned):
IEex_AllWorldScreenPanelIDs = {0,1,7,8,9,6,17,19,21,22} plus 23
(Quickloot), 50/51, and 100 when non-vanilla; the two helpers that
test ids directly pin CUIPanel's m_nID at +0x20 (PanelRClickRefonte
accepts ids 0 and 1; _CUIPanel_Render__CVidMosaic_Render skips id 0).

#### IEEX_HD_UI and the mutual exclusion

`local IEEX_HD_UI = false` (State:7), replaced by the
2x-UI component's REPLACE_TEXTUALLY. It and the refonte are MUTUALLY
EXCLUSIVE at install time: the HD-UI block writes
`mov [esp+0x1C],1` over RenderPortraitRect's same 7 prologue bytes
(patch comment: "until that forcing moves into the DLL"). The DLL
hedges: RenderPortraitRect itself forces `param_8 = 1` whenever
DAT_104df536 (HD-UI flag, written only by SetHDUI -- committed
ui-scale note) is set, so the byte-patch and the flag agree. The Lua
pre-divide pattern on this path:
`local div = (IEEX_HD_UI and IEex_ReadDword(IEex_GetUIManagerFromEngine(worldScreen) + 0xAA) ~= 0) and 2 or 1`.

#### The panel content-rect registry and the phantom-UI gate

Three helpers here (PanelMosaicRender, PanelRClickRefonte,
PanelTimerGated) read, and none writes, the registry the hud group
owns: 5-dword entries at DAT_103dec28 (stride 5: [id/type, x, y, w,
h]), count DAT_104df4dc, capacity 0x10, filled by
HudClearPanelContentRects / HudAddPanelContent. The committed
hud-group note documents the "Rect-table contract" and the staged
registration set (ids: 1 = portrait slots, 0 = degenerate keep-alive
whose w/h <= 0 draws nothing, -2 = log box, -3 = the bar block:
action bar, command row, statue buttons, quickloot toggle);
cross-reference it -- this group does not duplicate the contract.
The phantom-UI gate DAT_104df576 is armed by the ui-input helpers
and read by PanelTimerGated and (indirectly, inside
FUN_10032980) PanelRClickRefonte.

#### Tint BAMs and the hardcoded resrefs

- B3TINTG -- the green "can learn this scroll" tint; gate accepts
  "B3TINTG" or "STORTIN" + last byte in {'T','2','3','4'} (STORTINT
  red, STORTIN4 gold UMD, STORTIN2/STORTIN3). Read by
  CVidCell_RenderIconOverride (resref slots at cell+0xac/+0xb0 and
  cell+0xbc/+0xc0) and written by PostItemSlotRenderHook
  (ConstructFromResRef "B3TINTG").
- GUIRSPOR -- the portrait button bitmap whose PRESSED frame has the
  green selection rectangle baked into the ART (Lua comment, Bubb
  2026-07-25): stock 46x46 slot, rectangle at x/y 1 and 44; 2..3 and
  88..89 on the HD 2x copy. PortraitButtonCellRender's comment ties
  it to the CUIControlPortraitWorld skip.
- B3KEYS -- a panel MOS (SetPanelMosaicResref usage State:6192 /
  upstream:3635).

#### Threads and synchronization

All seventeen run on engine threads: the six exports from Lua
handlers on the engine/GUI thread; the forwards inside their hook
sites' render / input / timer call stacks. The only lock this group
touches is the CSingleLock bracket inside DecrementButtonDoRender
(sync object `*(*(button+6))+0x56`, bInitialOwn = 1) and the
GetShare/UndoShare pair (lock flags 0xffffffff, retry-on-4/5 loop)
inside the chargen override. Everything else is the fork's standard
lock-free-registry stance: single-threaded drawing assumed, no
atomics, no allocation on render paths.

#### Failure model and engine-address attribution

Every forward degrades to vanilla by construction: the gate misses ->
the original engine call runs unchanged (tail-jump, full forward, or
forward-then-skip); null pointers and unregistered bindings -> the
helper no-ops or skips its addition. The exports validate from the
Lua stack and return without pushing on their fail paths (Lua then
sees no values). Engine addresses beyond the registered slots are
attributed to the staged patch comments and this DLL's disassembly --
untrusted, never verified against the engine binary in this stream;
the hard-linked statics in RenderPortraitRect are the one deliberate
exception this group makes to the address-list discipline. The write
primitives the Lua stubs use (IEex_WriteAssembly, IEex_HookRestore,
IEex_HookJumpOnSuccess, IEex_HookReplaceFunctionMaintainOriginal,
IEex_HookJumpToAutoReturn, IEex_AttemptHook,
IEex_Disable/EnableRDataProtection) are the loader's PUBLIC API
(IEexDLL/IEexLua.asm) -- engine code patching, not fork internals.

`IEex_Helper_RenderPortraitRect` - 0x10033ce0 (FUN_10033ce0) -
stdcall, 8 stack args (param_1 = the stub-pushed ECX = CGameSprite
this; params 2-8 = the engine's original 7 args -- rect float* /
int* / flags / int / int / undefined / int), ret 0x20 (`c2 20 00`
byte-pin at VA 0x1003535a); entered from the IEex_Gui_Patch.lua stub
replacing CGameSprite::RenderPortrait 0x704D40, lines 1550-1583,
gated `if IEex_PortraitGridEnabled`.

##### Behavior

The DLL's full reimplementation of the portrait-frame draw (patch
comment: the hardcoded 42/44 geometry is derived from the control
size -- image = szControl-4, ring = szControl-2). 0x978-byte stack
frame, /GS cookie, ~650 lines of decompile; the pinned pipeline:

1. `if (DAT_104df536 != '\0') param_8 = 1;` -- the HD-UI force
   (writer: SetHDUI, committed ui-scale note); this is the DLL-side
   half of the IEEX_HD_UI / refonte mutual exclusion (COMMON).
2. Copies the caller's rect: left = *param_2, top = *param_3,
   right = left + param_3[1]... (the float*/int* pair), then
   `_DAT_008cf6dc` (g_pBaldurChitin global) and its +0x1c54
   CInfGame.
3. Early-out gate: `if ((*(_DAT_008cf6d8 + 0x96e) == 0) ||
   (*(_DAT_008cf6d8 + 0x104c) == *(param_1 + 0x62)))
   ... if (*(CInfGame + 0x43d8) != 0 && local_980 == 0) return;` --
   the 3D/HD-active state pair (g_pChitin statics +0x96e / +0x104c
   vs the sprite's +0x62) plus a CInfGame +0x43d8 flag; the exact
   semantics are the engine's, attributed to the patch comments.
4. Right/bottom anchors from the sprite: RStack_90c.right =
   param_1 + 0x53ea, .bottom = param_1 + 0x548a; font = "NOPORTSM"
   (dwords 0x4f504f4e / 0x4d535452) overridable by up to 8 chars
   from the `_DAT_008e706c` char* when set; local_975 =
   *(byte*)(param_1 + 0x5d0).
5. Frame thickness: `DAT_100927b0` -- the markers group's lazy
   "Portrait Frame Thickness" ini cache (default 1, clamp [1,4]);
   when < 1 and the 3D/HD flag `*(_DAT_008cf6d8 + 0x91c)` is set,
   auto = clamp((FUN_1002cfa0()+1)/2, 1, 4) -- the marker group's
   Selection-Circle-Thickness helper halved (the markers COMMON
   documents both caches).
6. Scale snap: `local_980 = DAT_104df564 < 1 || SCREENWIDTH == 0 ?
   1.0 : (float)SCREENWIDTH / (float)DAT_104df564` (the ui-scale
   sub-viewport width, committed UIScaleRenderBegin note);
   `pcStack_96c = DAT_103deda4 symmetric` with SCREENHEIGHT
   (_DAT_008ba31e); then the floor(x*scale+0.5)/scale snapping of
   every rect edge and the ceil-bounded strip width.
7. Color decision (pinned regions, prior sessions): the 0xffb7be /
   0xfa00 pair, FUN_1002d1e0 (the marker group's shared tint
   pipeline -- RenderPortraitRect "reuses steps 1-5 verbatim with
   its own inputs", per that note), the markers pair table, and
   FUN_1002d2f0 (NO decompile on disk in this stream -- flagged).
   The engine draw fallbacks 0x7b3190 with colors 0xb4b4b4 /
   0x1e1e78 / 0x32ff32 (hostile/grey/green markers) and the
   software path via *(panel vtable)+0x98 + FUN_1002d010.
8. HP text (the NUMFONT block): `local_975 = DAT_008f950d` and the
   `0 < (short)... && DAT_008f950c != '\0'` gate (the engine's
   NUMFONT gate bytes, hard-linked); 0x792ca0 / 0x793170
   ("NUMFONT", param_8, 1) / 0x7ae290 / 0x7adff0; the 4-entry
   threshold table `(&DAT_100882e8)[i] <= (hp*100)/max` picking
   colors from `(&UNK_100872d8)[i]`, default 0xff;
   `wsprintfA(buf, "%d/%d", hp, max)` or "-/-" when
   `uStack_954 & 0x8000000`; draws via 0x7930f0 / 0x793050 /
   0x793720; closes 0x77f820 / 0x792d10; final draw when
   `*(iVar17 + 0x7248) != 0`.
9. GL emission (the 3D path, this group's only GL): glEnable via
   DAT_104df4e4 (the first no-arg call in the decompile is a split
   artifact) then `DAT_104df4e4(0xbe2)` = glEnable(GL_BLEND);
   `DAT_103deffc` = glColor4f with the COLORREF 0x00BBGGRR bytes /
   255 and alpha 1.0; `DAT_104df220(7)` = glBegin(GL_QUADS); ~16
   `DAT_104df4f0` = glVertex2f calls (float args spilled/reloaded
   through the big frame -- geometry not fully decoded);
   `DAT_104df3fc` = glEnd; `DAT_104df024(0xde1)` =
   glDisable(GL_TEXTURE_2D). STATE LEAK: GL_BLEND left ON,
   GL_TEXTURE_2D left OFF (the render3d group's convention).
10. Cell-strip tails (LAB_10034ba6 / LAB_10033f33 neighborhood):
    strip list from context +0x7134 (<= 0x40 entries), engine
    helpers 0x7adf40 / 0x7ae290 / 0x7adff0, IntersectRect clip,
    the lost `(**(code**)(*piVar3+8))(...)` vtable+8 draw, and the
    stride math `_DAT_008f94b8 * stride` width / `_DAT_008f94bc`
    height with wrap (the hard-linked cell-strip multipliers); the
    8-dword zero loop with the FUN_10058959 range-check-cookie
    failure path closes the function.

The stub math (patch comment): __thiscall, 7 stack args, ret 0x1C;
`IEex_WriteAssembly(0x704D40, {!mark_esp; 7 reversed pushes of
[esp+0x1C..0x04]; 51 (push ecx); !call
>IEex_Helper_RenderPortraitRect; !ret_word 1C 00})` -- the helper's
ret 0x20 cleans its 8, the stub's ret 0x1C cleans the engine's
original 7 (60 bytes balanced).

##### Failure fallbacks

Null g_pChitin / gate misses -> plain return, engine draws nothing
extra (the whole helper IS the portrait draw in this mode -- the
hook replaced 0x704D40 outright, so "vanilla fallback" is the
software path inside the same helper). INI thickness 0 -> auto.
Scale divisors < 1 or zero width -> factor 1.0.

##### Decompiler artifact

`unaff_EDI` and the lost `pcStack_988` vtable slot are decompiler
losses in the strip tail (the +8 slot's identity is unpinned);
FUN_1002d2f0 and FUN_10035370 have no decompiles on disk (flagged
in the report); the ~16 glVertex2f calls merge into un-decodable
spill/reload sequences. Everything else above is byte- or
decompile-pinned as marked.

##### Lua wiring

Lines 1550-1583 (patch comment quoted in COMMON: modern-HUD portrait
busts, 42/44 geometry, __thiscall ret 0x1C, the HD-UI note that
IEex_UIScale_Patch.lua's `mov [esp+0x1C],1` writes over these same
7 prologue bytes), install via IEex_WriteAssembly at 0x704D40 (above).
Gated `if IEex_PortraitGridEnabled`; the paired consumer of its GL
output is PortraitButtonCellRender's skip (the bust is drawn HERE,
so the button must not draw it twice).

