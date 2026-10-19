# IEex_Helper_CScreenKeys_SetStagedKeybindOverride

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1001ebb0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1001ebb0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1001ebb0(int param_1,int param_2,char param_3,int param_4)

{
  int iVar1;
  int iVar2;
  char *pcVar3;
  undefined1 *puVar4;
  int in_ECX;
  uint uVar5;
  undefined4 local_20;
  undefined1 local_1c [4];
  undefined4 local_18;
  uint local_14;
  void *local_10;
  undefined1 *puStack_c;
  int local_8;
  byte bVar6;

  puStack_c = &LAB_1005bfbe;
  local_10 = ExceptionList;
  local_14 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  ExceptionList = &local_10;
  iVar1 = param_2 + param_1 * 0x3c;
  iVar2 = *(int *)(DAT_103d4728 + iVar1 * 0xc);
  if (iVar2 < 0) {
    iVar2 = *(int *)(DAT_103d4728 + 4 + iVar1 * 0xc);
    if (-1 < iVar2) {
      *(char *)(DAT_103d471c + 0x4b + iVar2 * 0x50) = param_3;
    }
  }
  else {
    *(char *)(iVar2 + 0x484 + in_ECX) = param_3;
  }
  iVar2 = *(int *)(DAT_103d4728 + iVar1 * 0xc);
  if (iVar2 < 0) {
    iVar1 = *(int *)(DAT_103d4728 + 4 + iVar1 * 0xc);
    if (-1 < iVar1) {
      *(char *)(DAT_103d471c + 0x4c + iVar1 * 0x50) = (char)param_4;
    }
  }
  else {
    *(char *)(iVar2 + 0x5f8 + in_ECX) = (char)param_4;
  }
  local_18 = DAT_100942f8;
  local_8 = 0;
  bVar6 = param_3 + 0xbf;
  uVar5 = (uint)CONCAT11(bVar6,param_3);
  if (param_4 == 0) {
    if (bVar6 < 0x1a) {
      puVar4 = *(undefined1 **)("index >= 0 && index < CINFGAME_KEYMAP_SIZE" + (uVar5 & 0xff) * 4);
    }
    else if ((byte)(param_3 - 0x30U) < 10) {
      puVar4 = *(undefined1 **)
                ("C:\\Projects\\Icewind2\\src\\Baldur\\InfScreenKeymaps.cpp" +
                (uVar5 & 0xff) * 4 + 0x28);
    }
    else {
      pcVar3 = &DAT_1008f168;
      do {
        if (*pcVar3 == param_3) {
          puVar4 = *(undefined1 **)(pcVar3 + 4);
          goto LAB_1001ed58;
        }
        pcVar3 = pcVar3 + 8;
      } while (pcVar3 != &DAT_1008f230);
      puVar4 = &DAT_10060a7d;
    }
LAB_1001ed58:
    (*DAT_100942f4)(&local_18,&DAT_100642cc,puVar4,local_14);
  }
  else {
    local_8._0_1_ = 2;
    local_8._1_3_ = 0;
    (*DAT_100941ec)(local_1c,0x603e);
    (*DAT_10094260)(local_1c);
    local_8._0_1_ = 3;
    (*DAT_10094158)();
    local_8._0_1_ = 1;
    if (bVar6 < 0x1a) {
      puVar4 = *(undefined1 **)("index >= 0 && index < CINFGAME_KEYMAP_SIZE" + (uVar5 & 0xff) * 4);
    }
    else if ((byte)(param_3 - 0x30U) < 10) {
      puVar4 = *(undefined1 **)
                ("C:\\Projects\\Icewind2\\src\\Baldur\\InfScreenKeymaps.cpp" +
                (uVar5 & 0xff) * 4 + 0x28);
    }
    else {
      pcVar3 = &DAT_1008f168;
      do {
        if (*pcVar3 == param_3) {
          puVar4 = *(undefined1 **)(pcVar3 + 4);
          goto LAB_1001eced;
        }
        pcVar3 = pcVar3 + 8;
      } while (pcVar3 != &DAT_1008f230);
      puVar4 = &DAT_10060a7d;
    }
LAB_1001eced:
    (*DAT_100942f4)(&local_18,&DAT_100649b4,local_20,puVar4);
    local_8._0_1_ = 4;
    (*DAT_10094158)();
    local_8 = (uint)local_8._1_3_ << 8;
  }
  (*DAT_1009445c)(0);
  (*DAT_100944d4)(param_2 + 0x10000005);
  (*DAT_100943ac)(&local_18);
  local_8 = 5;
  (*DAT_10094158)();
  ExceptionList = local_10;
  return;
}
```

## Notes

### Shared keys-screen machinery (all helpers in this group)

- **Two integration modes.** (1) Two Lua-callable registrations —
  `IEex_Helper_RegisterKeysScreenHardcodedMapDefaults` (0x1001cad0) and
  `IEex_Helper_RegisterKeysScreenCustomBindings` (0x1001cbb0) — called
  from the staged `IEex_Gui_State.lua` / `IEex_Gui_State.upstream.lua` at
  Lua-load time; they fill DLL-side data tables. (2) Twelve native hooks:
  the staged `IEex_Gui_Patch.lua` (inside `if not IEex_Vanilla then`)
  patches `!jmp_dword >IEex_Helper_<Name>` over each vanilla function
  body, so the helper *is* the engine function from then on. Both this
  patching and the `DAT_10094xxx` pointer registration ride the loader's
  PUBLIC primitives (`IEexDLL/IEexLua.asm`: `IEex_WriteAssembly` =
  `IEex_WriteByte`/`IEex_WriteByte_Count`, `IEex_AddressList`) — cite,
  do not reverse-engineer.
- **Hook sites** (`IEex_Gui_Patch.lua` lines 1463-1474):

  | Helper | Vanilla site | Trailing nops |
  |---|---|---|
  | CInfGame_InitHotkeyMapOverride | 0x5A98F0 | 2 |
  | CInfGame_UpdateKeybindMappingOverride | 0x5A9B00 | — |
  | CScreenKeys_InitDueToUserInteractionOverride | 0x639DC0 | — |
  | CScreenKeys_OnKeyDownProcessOverride | 0x639160 | 2 |
  | CScreenKeys_OnLButtonDownOverride | 0x639790 | — |
  | CScreenKeys_OnPanel1ButtonClickOverride | 0x63A480 | 2 |
  | CScreenKeys_SetSelectedDisplayIndexOverride | 0x63AA60 | 1 |
  | CScreenKeys_SetStagedKeybindOverride | 0x63A910 | 2 |
  | CScreenKeys_UpdateHoverPalettesOverride | 0x63A7A0 | — |
  | CScreenKeys_UpdateLabelPalettesOverride | 0x63A660 | 5 |
  | CScreenKeys_UpdateMainPanelOverride | 0x639A50 | 2 |
  | CUIControlButtonKeys_OnLButtonClickOverride | 0x63ACA0 | 1 |

- **Keymap split.** `CINFGAME_KEYMAP_SIZE` = 0x174 = 372 rows: 0..0x34
  (53) are non-spell keys, ini section "Keymap", key names from
  `DAT_10094368[i]`; 0x35..0x173 (319) are spell keys, key names from
  `DAT_100943c8[i-0x35]`, ini section "Priest Spells" while i < 0xc6 else
  "Wizard Spells".
- **CInfGame fields**: +0x4514 committed vk bytes (0x174), +0x4688
  committed ctrl bytes.
- **CScreenKeys fields** (`this`): +0x66 a dialog control ref (enabled
  via `DAT_10094448`); +0x456 dialog gate pointer (a dialog is open while
  `ptr != 0 && *(ptr+8) != 0`); +0x46a dialog state string ID (0x82c8 =
  duplicate-assign dialog, 0x82cc = overwrite-confirm dialog), +0x46e and
  +0x472 the other two dialog control IDs; +0x47e staged-ctrl byte;
  **+0x480 current page (int)**; +0x484 screen-local vk working copy
  (0x174 bytes); +0x5f8 screen-local ctrl working copy (0x174 bytes);
  +0x76c hovered row; +0x770 selected cell (-1 = none); +0x774/+0x775
  saved duplicate (vk, ctrl); +0x776 toggle byte persisted to
  options+0xe92.
- **Root map (`DAT_10094384`)**: +0x1c50 input object (its +0x9fe flag is
  set on LButtonDown), +0x1c54 CInfGame*, +0x1c78 options, +0x1ca8
  CScreenKeys* (engine assert "pKeymaps != NULL"), +0x3c4 engine object,
  +0x1906/+0x190a saved cursor x/y — restored whenever a dialog closes.
- **DLL-side tables**: `DAT_10090470` = the 372 hardcoded default key
  names as `std::string`s (stride 0x18, raw end pointer 0x10092750);
  `DAT_10094368` / `DAT_100943c8` = the ini key-name `char*` arrays;
  `DAT_1008f168`..0x1008f230 = the 25-entry `{byte vk, char* name}` table
  used by both the VK parser and formatter; `DAT_10094500` = page count.
- **Custom-binding vector** `DAT_103d471c/20/24` (begin/end/capacity),
  0x50-byte elements: +0x00 `iniSection` (`std::string`), +0x18 `iniKey`,
  +0x30 `default`, +0x48 `disallowModifiers` (bool), +0x49/+0x4a
  committed vk/ctrl, +0x4b/+0x4c staged vk/ctrl. Destroy `FUN_1001f970`;
  growth `FUN_1001fa70` (1.5x, max 0x3333333 elements, allocations
  >= 0x1000 get +0x23 bytes and 32-alignment, element move
  `FUN_1001ff60`).
- **Layout vector** `DAT_103d4728/2c/30`, 0xc-byte elements
  `{int hardcodedMapIndex, int customMapIndex, int strref}`; -1 = none;
  both -1 = filler row. Rebuilt at every screen open from the Lua
  callback **IEex_Extern_GiveEngineKeysScreenLayout** (`FUN_1001cf50`);
  page count `DAT_10094500 = ceil(rows/60)`; one page is 60 rows =
  0x2d0 bytes of vector.
- **VK name machinery.** Parse `FUN_1001ced0`: "" -> 0; 'a'-'z' -> the
  'A'-'Z' vk (subtract 0x20); 'A'-'Z' and '0'-'9' as-is; else a
  case-sensitive scan of the 25-name table; no match -> 0. Format:
  `(byte)(vk+0xbf) < 0x1a` -> 'A'..'Z'; `(byte)(vk-0x30) < 10` ->
  '0'..'9'; else the 25-name scan; else "" (`DAT_10060a7d`).
- **ini persistence.** All access is `.\Keymap.ini`. Reads go through
  `GetPrivateProfileStringA` into the 0x80 scratch `DAT_10094200` with
  the registered default as the default; all three prefix spellings
  "CTRL-"/"ctrl-"/"Ctrl-" are accepted (`DAT_100942fc` probe, result 0 =
  needle at start; the name is parsed from value+5). Values are written
  back canonically as "CTRL-"+name; writing an empty value deletes the
  ini key.
- **Commit + Lua notify.** `FUN_1001d2c0` walks the custom vector: any
  element whose staged bytes differ from committed is committed
  (+0x49 <- +0x4b, +0x4a <- +0x4c), persisted
  (`WritePrivateProfileStringA(iniSection, iniKey, "CTRL-"+name or name,
  ".\Keymap.ini")`), and described in a Lua table
  `{customMapIndex, oldVirtualKey, oldVirtualKeyHasCtrl, newVirtualKey,
  newVirtualKeyHasCtrl}` (the staged Lua ORs 0x100 into the vk when
  ctrl). If anything changed, `FUN_1001c940` invokes
  **IEex_Extern_KeysScreenOnCustomBindingsChanged** — a protected call
  (two `lua_getfield` lookups, caller-built MSVC `std::function`,
  `lua_pcall`; a Lua error becomes
  `LogVariadic("[!][IEexHelper.dll] luaCallProtected() - %s\n", err)`).
- **Engine-pointer registry.** Every `DAT_10094xxx` symbol used below is
  a BSS pointer the host fills at load through the loader's public
  address-list mechanism (`IEexDLL/IEexLua.asm`, `IEex_AddressList`) —
  cite, do not RE. Roles: `DAT_10094158` UI-op flush/commit;
  `DAT_100941ec` get game string by strref; `DAT_10094260` set control
  text; `DAT_1009421c`/`DAT_10094190` set label by name/value;
  `DAT_100941f4`/`DAT_10094194` end/begin UI-op batch;
  `DAT_10094278` select cell (0xffffffff clears);
  `DAT_100942b8` get `lua_State`; `DAT_100942bc` engine assert
  `(line, "C:\Projects\Icewind2\src\Baldur\InfScreenKeymaps.cpp", expr,
  0)`; `DAT_100942d4` cursor apply/restore; `DAT_100942f4` sprintf-style
  format; `DAT_100942f8` scratch `std::string`; `DAT_100942fc` substring
  probe; `DAT_100943ac` attach string to control; `DAT_100943c4` set
  static text; `DAT_10094350`/`DAT_10094374`/`DAT_100943e4`/
  `DAT_100943e8` dialog handlers; `DAT_100943f0` screen pre-init;
  `DAT_10094448` enable/disable control; `DAT_1009445c` select panel;
  `DAT_10094464` cursor set; `DAT_10094484` end batch; `DAT_10094494`
  render a keybind on a cell; `DAT_100944d4` get control by ID;
  `DAT_100944d8` forward a mouse point; `DAT_100944e0` push a keymap row
  into the engine; `DAT_100941c4`/`FUN_10014900` std::string assign;
  `DAT_100942ac` strlen.
- **Control-ID scheme.** Key-list cells are controls
  `0x10000005 + page-relative row`; action-list cells are
  `0x10000041 + row` (key-list base + 0x3c); page navigation touches
  `0x10000001`; the page-caption static is 0x82cb; assign-dialog controls
  0x82c8/0x82c9/0x82ca, confirm-dialog 0x82cc/0x82c9/0x82ca; the
  empty-cell prompt text is the "_" literal (0x10064af8); the page labels
  are "PAGE"/"NUMPAGES" (0x10064b00 and by name) with `"%d"` values.
- **No GL.** Nothing in this group calls OpenGL — it is UI, ini and Lua
  only. Renderer relevance is input-path context only.
### Fork load-time context (all helpers)

- **InitHelperDLL** (the DLL's ordinal-1 export at `0x1000e260`, stdcall,
  `ret 0xc`) is called by the host at load: its first argument is ignored,
  its second is stored into `DAT_100944fc` — a host-provided logging
  function pointer the fork calls through all over its code — and its
  third into `DAT_100944f8`, the sink the DLL's exported `LogVariadic`
  forwards to. It also captures the epoch base used by the time helpers.
- **LogVariadic** (ordinal-4 export at `0x1000e1b0`): variadic
  `(format, ...)`. If `DAT_100944f8` is still NULL the call is a no-op;
  otherwise it takes the global recursive mutex (mtx `0x1008f138`,
  recursion count `0x1008f164`; `_Mtx_lock` failure -> `_Throw_Cpp_error(5)`,
  count `0x7fffffff` -> set `0x7ffffffe` and throw 6) and calls
  `sink(format, &va_list)`.
- **Dual registration.** The fork's init function fills a Lua table with
  `"IEex_Helper_<Name>" -> <raw code address>` entries (for IEex `!call`
  raw-call macros), and exported `ExposeFunctions` registers callable
  `lua_CFunction` shims under the same names.
##### Behavior

Caller: the staged `IEex_Gui_Patch.lua` line 1470 — hooks vanilla
`CScreenKeys::SetStagedKeybind` at 0x63A910 (+2 nops).

`(page, cell, vk, ctrl)` — stage a binding on one row and show it.

- Absolute row = cell + page*0x3c. The vk byte goes to `this+0x484[row]`
  for a hardcoded row or custom staged +0x4b (filler: no write); the ctrl
  byte likewise (+0x5f8 / +0x4c).
- Builds the display text: VK name via the shared format chain; when ctrl
  is set the prefix comes from game strref 0x603e (`DAT_100941ec`) and
  the text is formatted "%s%s" (prefix + name), otherwise plain "%s".
- `DAT_1009445c(0)`, fetch control `cell + 0x10000005`, attach the text
  (`DAT_100943ac`), flush.

##### Failure fallbacks

An unknown vk renders the empty string (the cell shows as unbound). No
validation of page/cell — the row index is used raw into the working
copies and the layout vector.

##### Decompiler artifact

An MSVC exception-state variable (0..5) threads the string build; the
strref fetch + format appear as separate UI-op calls with the stack
cookie as a phantom argument.

