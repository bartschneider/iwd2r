# IEex_Helper_CScreenKeys_OnKeyDownProcessOverride

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1001deb0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1001deb0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1001deb0(byte param_1)

{
  char cVar1;
  byte bVar2;
  char cVar3;
  int iVar4;
  byte *pbVar5;
  undefined1 *puVar6;
  int *in_ECX;
  int *piVar7;
  int iVar8;
  undefined4 uVar9;
  undefined4 local_30;
  int local_2c;
  uint local_28;
  uint local_24;
  uint local_20;
  int local_1c;
  uint local_18;
  uint local_14;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005bef9;
  local_10 = ExceptionList;
  local_14 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  local_24 = 0;
  local_30 = CONCAT31(local_30._1_3_,param_1);
  iVar8 = *DAT_10094384;
  local_20 = *(uint *)(iVar8 + 0x1c54);
  local_1c = iVar8;
  if (param_1 == 0x1b) {
    if ((*(int *)((int)in_ECX + 0x456) != 0) && (*(int *)(*(int *)((int)in_ECX + 0x456) + 8) != 0))
    {
      ExceptionList = &local_10;
      (*DAT_10094350)();
      ExceptionList = local_10;
      return;
    }
    if (in_ECX[0x1dc] < 0) {
      ExceptionList = &local_10;
      in_ECX[0x1dc] = -1;
      if (*(int *)(iVar8 + 0x1c78) == 0) {
        (*DAT_100942bc)(0x4b0,"C:\\Projects\\Icewind2\\src\\Baldur\\InfScreenKeymaps.cpp",
                        "pOptions != NULL",0);
      }
      *(undefined1 *)(*(int *)(iVar8 + 0x1c78) + 0xe92) = *(undefined1 *)((int)in_ECX + 0x776);
      (**(code **)(*in_ECX + 0x2c))(*(undefined4 *)(iVar8 + 0x1c78));
      ExceptionList = local_10;
      return;
    }
  }
  else {
    if (param_1 != 0xd) {
      ExceptionList = &local_10;
      iVar4 = (*DAT_100941dc)(local_30,local_14);
      if (iVar4 != 0) {
        ExceptionList = local_10;
        return;
      }
      if (*(char *)((int)in_ECX + 0x47e) != '\0') {
        if (param_1 == 9) {
          (*DAT_10094450)();
        }
        else if (param_1 == 0x2c) {
          (**(code **)(**(int **)(*(int *)(iVar8 + 0x3c4) + 4) + 0xec))();
        }
      }
      if ((*(int *)((int)in_ECX + 0x456) == 0) ||
         (iVar4 = *(int *)(*(int *)((int)in_ECX + 0x456) + 8), iVar4 == 0)) {
        iVar4 = in_ECX[0x1dc];
        if (-1 < iVar4) {
          if ((param_1 == 8) || (param_1 == 0x2e)) {
            param_1 = 0;
            local_2c = (uint)local_2c._1_3_ << 8;
          }
          else if (((param_1 < 0x30) || (0x39 < param_1)) && ((param_1 < 0x41 || (0x5a < param_1))))
          {
            if (9 < (byte)(param_1 - 0x30)) {
              pbVar5 = &DAT_1008f168;
              do {
                if (*pbVar5 == param_1) break;
                pbVar5 = pbVar5 + 8;
              } while (pbVar5 != &DAT_1008f230);
            }
            param_1 = FUN_1001ced0();
            local_2c = CONCAT31(local_2c._1_3_,param_1);
            if (param_1 == 0) {
              ExceptionList = local_10;
              return;
            }
            iVar4 = in_ECX[0x1dc];
          }
          else {
            local_2c = CONCAT31(local_2c._1_3_,param_1);
          }
          cVar1 = *(char *)((int)in_ECX + 0x47e);
          if (((cVar1 != '\0') && (0x2f < param_1)) && (param_1 < 0x3a)) {
            ExceptionList = local_10;
            return;
          }
          if (param_1 != 0) {
            local_28 = 0;
            local_20 = iVar4 + in_ECX[0x120] * 0x3c;
            local_18 = (DAT_103d472c - DAT_103d4728) / 0xc;
            iVar8 = local_1c;
            if (local_18 != 0) {
              piVar7 = (int *)(DAT_103d4728 + 4);
              do {
                iVar4 = piVar7[-1];
                if (iVar4 < 0) {
                  if (-1 < *piVar7) {
                    bVar2 = *(byte *)(DAT_103d471c + 0x4b + *piVar7 * 0x50);
                    goto LAB_1001e1d3;
                  }
                }
                else {
                  bVar2 = *(byte *)((int)in_ECX + iVar4 + 0x484);
LAB_1001e1d3:
                  if (bVar2 == param_1) {
                    if (iVar4 < 0) {
                      if (*piVar7 < 0) {
                        cVar3 = '\0';
                      }
                      else {
                        cVar3 = *(char *)(DAT_103d471c + 0x4c + *piVar7 * 0x50);
                      }
                    }
                    else {
                      cVar3 = *(char *)((int)in_ECX + iVar4 + 0x5f8);
                    }
                    if ((cVar3 == cVar1) && (local_28 != local_20)) {
                      *(byte *)(in_ECX + 0x1dd) = param_1;
                      *(char *)((int)in_ECX + 0x775) = cVar1;
                      if (cVar1 == '\0') {
                        (*DAT_100943c4)(&DAT_10060a7d);
                        local_8 = 1;
                        local_24 = 2;
                        (*DAT_10094260)(&local_30);
                        piVar7 = &local_2c;
                        local_8 = 2;
                        local_24 = 6;
                      }
                      else {
                        piVar7 = (int *)FUN_1001ca50();
                        local_8 = 0;
                        local_24 = 1;
                      }
                      (*DAT_10094260)(piVar7);
                      local_8 = 0;
                      if ((local_24 & 4) != 0) {
                        local_24 = local_24 & 0xfffffffb;
                        local_8 = 5;
                        (*DAT_10094158)();
                      }
                      if ((local_24 & 2) != 0) {
                        local_24 = local_24 & 0xfffffffd;
                        local_8 = CONCAT31(local_8._1_3_,7);
                        (*DAT_10094158)();
                      }
                      if ((local_24 & 1) != 0) {
                        local_24 = local_24 & 0xfffffffe;
                        local_8 = CONCAT31(local_8._1_3_,9);
                        (*DAT_10094158)();
                      }
                      local_18 = DAT_100942f8;
                      local_8 = CONCAT31(local_8._1_3_,10);
                      if ((byte)(param_1 + 0xbf) < 0x1a) {
                        puVar6 = *(undefined1 **)
                                  ("index >= 0 && index < CINFGAME_KEYMAP_SIZE" +
                                  (CONCAT11(cVar1,param_1) & 0xff) * 4);
                        goto LAB_1001e36f;
                      }
                      if (9 < (byte)(param_1 - 0x30)) {
                        pbVar5 = &DAT_1008f168;
                        goto LAB_1001e357;
                      }
                      puVar6 = *(undefined1 **)
                                ("C:\\Projects\\Icewind2\\src\\Baldur\\InfScreenKeymaps.cpp" +
                                (CONCAT11(cVar1,param_1) & 0xff) * 4 + 0x28);
                      goto LAB_1001e36f;
                    }
                  }
                }
                local_28 = local_28 + 1;
                piVar7 = piVar7 + 3;
              } while (local_28 < local_18);
            }
          }
          (*DAT_10094494)(in_ECX[0x120],in_ECX[0x1dc],local_2c,*(undefined1 *)((int)in_ECX + 0x47e))
          ;
          (*DAT_10094278)(0xffffffff);
          (*DAT_100942d4)(*(undefined4 *)(iVar8 + 0x1906),*(undefined4 *)(iVar8 + 0x190a));
          ExceptionList = local_10;
          return;
        }
        switch(param_1) {
        case 0xd:
        case 0x44:
          uVar9 = 4;
          break;
        default:
          ExceptionList = local_10;
          return;
        case 0x1b:
        case 0x43:
          uVar9 = 5;
          break;
        case 0x21:
        case 0x25:
          uVar9 = 0;
          break;
        case 0x22:
        case 0x27:
          uVar9 = 1;
        }
        (*DAT_1009445c)(0);
        piVar7 = (int *)(*DAT_100944d4)(uVar9);
      }
      else {
        if (*(int *)(iVar4 + 0x20) != 1) {
          ExceptionList = local_10;
          return;
        }
        switch(param_1) {
        case 0xd:
        case 0x59:
          piVar7 = (int *)(*DAT_100944d4)(1);
          break;
        default:
          ExceptionList = local_10;
          return;
        case 0x1b:
        case 0x4e:
          piVar7 = (int *)(*DAT_100944d4)(2);
        }
      }
      if (piVar7 == (int *)0x0) {
        ExceptionList = local_10;
        return;
      }
      (**(code **)(*piVar7 + 0x68))(0,0);
      ExceptionList = local_10;
      return;
    }
    if ((*(int *)((int)in_ECX + 0x456) != 0) && (*(int *)(*(int *)((int)in_ECX + 0x456) + 8) != 0))
    {
      return;
    }
    if (in_ECX[0x1dc] < 0) {
      ExceptionList = &local_10;
      in_ECX[0x1dc] = -1;
      (**(code **)(*in_ECX + 0x2c))(*(undefined4 *)(iVar8 + 0x1c78));
      iVar8 = 0;
      do {
        (*DAT_100944e0)(iVar8,*(undefined1 *)((int)in_ECX + iVar8 + 0x484),
                        *(undefined1 *)((int)in_ECX + iVar8 + 0x5f8));
        iVar8 = iVar8 + 1;
      } while (iVar8 < 0x174);
      FUN_1001d2c0();
      ExceptionList = local_10;
      return;
    }
  }
  ExceptionList = &local_10;
  (*DAT_10094278)(0xffffffff);
  (*DAT_100942d4)(*(undefined4 *)(iVar8 + 0x1906),*(undefined4 *)(iVar8 + 0x190a));
  ExceptionList = local_10;
  return;
  while (pbVar5 = pbVar5 + 8, pbVar5 != &DAT_1008f230) {
LAB_1001e357:
    if (*pbVar5 == param_1) {
      puVar6 = *(undefined1 **)(pbVar5 + 4);
      goto LAB_1001e36f;
    }
  }
  puVar6 = &DAT_10060a7d;
LAB_1001e36f:
  (*DAT_100942f4)(&local_18,&DAT_100649b4,local_20,puVar6);
  (*DAT_1009421c)(&DAT_10064ae8);
  (*DAT_10094190)(&local_18);
  local_8 = 0xc;
  (*DAT_100941ec)(&local_1c,*(undefined4 *)(DAT_103d4728 + 8 + local_28 * 0xc));
  (*DAT_10094260)(&local_1c);
  local_24 = local_24 | 0x10;
  local_8 = 0xd;
  (*DAT_10094158)();
  local_8 = 0xe;
  (*DAT_1009421c)("OPERATION");
  (*DAT_10094190)(&local_28);
  local_8._0_1_ = 0xf;
  (*DAT_10094158)();
  local_8._0_1_ = 10;
  *(undefined4 *)((int)in_ECX + 0x46a) = 0x82c8;
  *(undefined4 *)((int)in_ECX + 0x46e) = 0x82c9;
  *(undefined4 *)((int)in_ECX + 0x472) = 0x82ca;
  (*DAT_10094374)(1);
  local_8 = CONCAT31(local_8._1_3_,0x10);
  (*DAT_10094158)();
  local_8 = 0x11;
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

Caller: the staged `IEex_Gui_Patch.lua` line 1466 — hooks vanilla
`CScreenKeys::OnKeyDownProcess` at 0x639160 (+2 nops).

The keys-screen keyboard dispatcher; the root map `DAT_10094384` supplies
the input object (+0x1c50), options (+0x1c78) and CInfGame (+0x1c54).
Everything below is gated behind the dialog-open test
`this+0x456 != 0 && *(this+0x456)+8 != 0`.

- ESC with a dialog open -> `DAT_10094350` (dialog cancel handler); ESC
  with no selection -> virtual `(*this+0x2c)(options)` screen close.
- ',' (0x2c) while the staged-ctrl byte +0x47e is set -> re-render the
  staged text on the selected cell:
  `DAT_10094494(page = +0x480, cell = +0x770, vk, ctrl = +0x47e)`.
- ENTER with a selection: virtual close(options), then the commit loop
  i = 0..0x173 of `DAT_100944e0(i, +0x484[i], +0x5f8[i])` (push the whole
  working copy into the engine display), then `FUN_1001d2c0` (commit
  custom slots, persist changed ones to `.\Keymap.ini`, notify Lua).
- BACKSPACE / DELETE -> staged vk = 0 (unbind).
- Digit keys are rejected while +0x47e (staged ctrl) is set.
- Duplicate detection: when the staged (vk, ctrl) matches another row,
  that row's values are saved to +0x774/+0x775 and the duplicate-assign
  dialog is opened (control string IDs 0x82c8/0x82c9/0x82ca, row strref,
  text fragments "KEY"/"ATION").
- With no selection: ENTER/'D' -> control 4, ESC/'C' -> control 5,
  PAGE UP/LEFT -> control 0, PAGE DOWN/RIGHT -> control 1 (fetched with
  `DAT_100944d4`, activated via vtable +0x68 with (0,0)).
- Message-box mode (`*(dialog+0x20) == 1`): ENTER/'Y' -> 1, ESC/'N' -> 2.

##### Failure fallbacks

None; unrecognized keys fall through to the vanilla-equivalent default
branches.

##### Decompiler artifact

`in_ECX[0x120]` and `piVar1[0x1dc]` are dword-index renderings of the
byte offsets +0x480 (page) and +0x770 (selected cell); dialog-gate and
vtable calls show as raw double dereferences.

