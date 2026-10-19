# IEex_Helper_CInfCursor_SetToolTipOverride

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1001d700` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1001d700.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1001d700(int *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                 char *param_5,undefined4 param_6,undefined4 param_7)

{
  int iVar1;
  char cVar2;
  int in_ECX;
  undefined4 local_18;
  uint local_14;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005bdfd;
  local_10 = ExceptionList;
  local_14 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  ExceptionList = &local_10;
  if (*param_1 != -1) {
    iVar1 = *(int *)(*DAT_10094384 + 0x1c54);
    if (*(int *)(iVar1 + 0x449e) == 0) {
      (*DAT_1009437c)(&DAT_10060a7d);
    }
    else {
      cVar2 = FUN_1001d570(local_14);
      if (cVar2 == '\0') {
        FUN_1001d570();
      }
      if (*param_5 != '\0') {
        local_18 = DAT_100942f8;
        local_8 = 0;
        (*DAT_100942f4)(&local_18," (%s)",param_5);
        (*DAT_10094190)(&local_18);
        local_8 = 1;
        (*DAT_10094158)();
        local_8 = 0xffffffff;
      }
    }
    (*DAT_100942d0)(param_5,param_1,param_6,param_7);
    *(undefined4 *)(in_ECX + 0xa02) = param_2;
    *(undefined1 *)(iVar1 + 0x1ba1) = 0x65;
  }
  ExceptionList = local_10;
  return;
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

`IEex_Helper_CInfCursor_SetToolTipOverride` - 0x1001d700
(FUN_1001d700) - thiscall-shaped stdcall: ECX = CInfCursor, 7 stack
args, ret 0x1C (`c2 1c 00`); entered from the IEex_Gui_Patch.lua
whole-replace at engine 0x597740 (CInfCursor::SetToolTip), line 1462,
opening the "-- Extend CScreenKeys (GUIKEYS.CHU) to allow for custom
keybinds --" block.

##### Behavior

Fully hex-decoded from the raw bytes (raw 0x101eb00 region; every
pin below is from the disassembly, the Ghidra decompile mangles the
first resolve call). Extends the engine tooltip so keybind names
resolve through the custom keymap chain:

1. `esi = param_1` (the tooltip id/source int); gate
   `cmp [esi],-1` / je end -- id -1 exits untouched.
2. `edi = *(*DAT_10094384 + 0x1c54)` = the CInfGame.
3. `cmp [edi+0x449e],0` / je -> when the custom-keybind flag is
   CLEAR, the clear path: push 0x10060a7d (the .rdata EMPTY string,
   all zero bytes) and `CString::SetFromChars` (DAT_1009437c,
   ECX = &param_5) blanks param_5, then jump to the tail.
4. Resolve path: `FUN_1001d570(ECX = param_3, EDX = &param_5)` -- a
   keymap-lookup helper taking the keymap selector in ECX and the
   OUT CString in EDX (`8b 4d 10 / 8d 55 18 / e8 fa fd ff ff`).
   If it produced an empty string (`test al,al` after the call's
   bool return), retry once with param_4:
   `FUN_1001d570(ECX = param_4, EDX = &param_5)`.
5. Append block: when param_5 is non-empty, build a temp CString =
   `Format(" (%s)", <global CString DAT_100942f8>)` -- note the
   format string 0x10064a6c is literally `" (%s)"` and the inserted
   value is the GLOBAL slot, not param_5 -- then assign the temp
   into param_5 through the DAT_10094190 slot (bound
   "CString::SetFromCString" but used assign/append-shaped), then
   destruct the temp (DAT_10094158). The self-referencing-looking
   Format + assign pair is documented mechanically; its net effect
   is param_5 = keyName + " (" + globalSlot + ")".
6. Tail: `CInfToolTip::FormatTooltipPrefix` (DAT_100942d0) called as
   this = `cursor + 0x396` (`lea ecx,[ebx+0x396]`) with args
   (param_5, param_1, param_6, param_7) pushed in that order --
   the cursor's own tooltip prefix field is the `this`.
7. `[cursor + 0xa02] = param_2`; `byte [CInfGame + 0x1ba1] = 0x65`
   (a fixed flag/state byte write, purpose not decoded).
8. The engine's original body (from 0x597740) never runs -- this is
   a whole-replace.

##### Failure fallbacks

Id -1 -> no work. Custom-keybind flag clear -> param_5 blanked and
the prefix path still runs (so the vanilla tooltip shape is
preserved). Both keymap lookups empty -> param_5 stays empty, the
append block is skipped, the tail still formats the prefix. Null
CInfGame is NOT guarded (`edi` is dereferenced unconditionally after
the +0x1c54 read) -- the registered binding is assumed present (the
attemptFillPointer contract).

##### Decompiler artifact

The decompile's first resolve call is mangled (the `lea ecx,[ebp+0x10]`
landing in the wrong slot); the byte pins above are authoritative.
Register-role CONFLICT with the committed keys-screen note: there,
FUN_1001d570 is recorded with the out-CString and selector roles on
the other registers; THIS call site proves ECX = selector, EDX =
&out. Both notes quote their evidence mechanically; the discrepancy
is flagged here and in the report, and the committed note is NOT
rewritten from this group.

##### Lua wiring

Line 1462: `IEex_WriteAssembly(0x597740, { "!jmp_dword
>IEex_Helper_CInfCursor_SetToolTipOverride !repeat(2,!nop)" })` --
whole-replace (5-byte jmp + 2 nops over the prologue), opening the
custom-keybind block; the same block hardcodes the CScreenKeys
control sites (0x5A98F0/0x5A9B00/0x639DC0/0x639160/0x639790/0x63A480/
0x63AA60/0x63A910). The helper's ret 0x1C cleans the engine's
original 7 stack args, matching SetToolTip's own `ret 0x1C`.

