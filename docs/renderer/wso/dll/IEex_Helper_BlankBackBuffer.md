# IEex_Helper_BlankBackBuffer

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1003a590` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1003a590.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_1003a590(int param_1)

{
  byte bVar1;
  int iVar2;
  bool bVar3;
  HMODULE hModule;
  FARPROC pFVar4;
  undefined *puVar5;
  int iVar6;
  undefined *puVar7;
  undefined4 *puVar8;
  BOOL BVar9;
  HANDLE pvVar10;
  undefined2 *puVar11;
  UINT UVar12;
  undefined4 uVar13;
  undefined4 uVar14;
  uint uVar15;
  undefined *puVar16;
  undefined1 uVar17;
  undefined8 uVar18;
  undefined1 *puVar19;
  SIZE_T SVar20;
  int local_14;
  DWORD DStack_10;
  uint local_c;

  local_c = DAT_1008f040 ^ (uint)&local_14;
  local_14 = param_1;
  if (DAT_104df768 == '\0') {
    hModule = GetModuleHandleA("opengl32.dll");
    if (hModule != (HMODULE)0x0) {
      pFVar4 = GetProcAddress(hModule,"glGetString");
      if (pFVar4 != (FARPROC)0x0) {
        puVar5 = (undefined *)(*pFVar4)(0x1f00);
        iVar6 = (*pFVar4)(0x1f01);
        puVar7 = (undefined *)(*pFVar4)(0x1f02);
        if (iVar6 != 0) {
          DAT_104df768 = '\x01';
          puVar16 = &DAT_10065c64;
          if (puVar7 != (undefined *)0x0) {
            puVar16 = puVar7;
          }
          puVar7 = &DAT_10065c64;
          if (puVar5 != (undefined *)0x0) {
            puVar7 = puVar5;
          }
          LogVariadic("[GLINFO] vendor=%s | renderer=%s | version=%s",puVar7,iVar6,puVar16);
        }
      }
    }
  }
  FUN_10030290();
  FUN_10039fe0();
  if ((((DAT_104df6a8 == '\0') && (DAT_104df6a8 = '\x01', DAT_0066d5a0 == 'j')) &&
      (DAT_0066d5a1 == -1)) && (DAT_0066d5a2 == 'h')) {
    puVar8 = VirtualAlloc((LPVOID)0x0,0x60,0x3000,0x40);
    if (puVar8 != (undefined4 *)0x0) {
      *puVar8 = 0x24448b60;
      puVar8[1] = 0xb8515024;
      puVar8[2] = &LAB_1003a260;
      *(undefined2 *)(puVar8 + 3) = 0xd0ff;
      *(undefined1 *)((int)puVar8 + 0xe) = 0x61;
      *(uint *)((int)puVar8 + 0xf) =
           CONCAT13(uRam0066d5a3,CONCAT12(DAT_0066d5a2,CONCAT11(DAT_0066d5a1,DAT_0066d5a0)));
      *(ushort *)((int)puVar8 + 0x13) = CONCAT11(DAT_0066d5a5,DAT_0066d5a4);
      *(undefined1 *)((int)puVar8 + 0x15) = DAT_0066d5a6;
      *(undefined1 *)((int)puVar8 + 0x16) = 0xe9;
      *(int *)((int)puVar8 + 0x17) = 0x66d58c - (int)puVar8;
      BVar9 = VirtualProtect(&DAT_0066d5a0,7,0x40,&DStack_10);
      if (BVar9 != 0) {
        DAT_0066d5a5 = 0x90;
        DAT_0066d5a6 = 0x90;
        DAT_0066d5a0 = -0x17;
        iVar6 = (int)puVar8 + -0x66d5a5;
        DAT_0066d5a1 = (char)iVar6;
        DAT_0066d5a2 = (char)((uint)iVar6 >> 8);
        uRam0066d5a3 = (undefined1)((uint)iVar6 >> 0x10);
        DAT_0066d5a4 = (undefined1)((uint)iVar6 >> 0x18);
        VirtualProtect(&DAT_0066d5a0,7,DStack_10,&DStack_10);
        SVar20 = 7;
        puVar19 = &DAT_0066d5a0;
        pvVar10 = GetCurrentProcess();
        FlushInstructionCache(pvVar10,puVar19,SVar20);
        LogVariadic(
                   "[SPELLICON] CUIControlButtonSpellbookSpell::SetSpell invalidate-on-change installed @0x66D5A0"
                   );
      }
    }
  }
  if (((DAT_104df700 == '\0') && (DAT_104df700 = '\x01', DAT_0066dc30 == 'j')) &&
     ((DAT_0066dc31 == -1 && (DAT_0066dc32 == 'h')))) {
    puVar11 = VirtualAlloc((LPVOID)0x0,0x60,0x3000,0x40);
    if (puVar11 != (undefined2 *)0x0) {
      *puVar11 = 0x5160;
      *(undefined1 *)(puVar11 + 1) = 0xb8;
      *(code **)((int)puVar11 + 3) = FUN_100124e0;
      *(undefined2 *)((int)puVar11 + 7) = 0xd0ff;
      *(undefined1 *)((int)puVar11 + 9) = 0x61;
      *(uint *)(puVar11 + 5) =
           CONCAT13(uRam0066dc33,CONCAT12(DAT_0066dc32,CONCAT11(DAT_0066dc31,DAT_0066dc30)));
      puVar11[7] = CONCAT11(DAT_0066dc35,DAT_0066dc34);
      *(undefined1 *)(puVar11 + 8) = DAT_0066dc36;
      *(undefined1 *)((int)puVar11 + 0x11) = 0xe9;
      *(int *)(puVar11 + 9) = 0x66dc21 - (int)puVar11;
      BVar9 = VirtualProtect(&DAT_0066dc30,7,0x40,&DStack_10);
      if (BVar9 != 0) {
        DAT_0066dc35 = 0x90;
        DAT_0066dc36 = 0x90;
        DAT_0066dc30 = -0x17;
        iVar6 = (int)puVar11 + -0x66dc35;
        DAT_0066dc31 = (char)iVar6;
        DAT_0066dc32 = (char)((uint)iVar6 >> 8);
        uRam0066dc33 = (undefined1)((uint)iVar6 >> 0x10);
        DAT_0066dc34 = (undefined1)((uint)iVar6 >> 0x18);
        VirtualProtect(&DAT_0066dc30,7,DStack_10,&DStack_10);
        SVar20 = 7;
        puVar19 = &DAT_0066dc30;
        pvVar10 = GetCurrentProcess();
        FlushInstructionCache(pvVar10,puVar19,SVar20);
        LogVariadic(
                   "[SPELLICON] CUIControlButtonSpellbookSpell::Render icon-cache warm installed @0x66DC30"
                   );
      }
    }
  }
  if (((DAT_104df755 == '\0') && (DAT_104df755 = '\x01', _DAT_00626352 == -0x3174e975)) &&
     ((CONCAT11(DAT_00626357,DAT_00626356) == 0x52ff && (DAT_00626358 == 'P')))) {
    puVar11 = VirtualAlloc((LPVOID)0x0,0x40,0x3000,0x40);
    if (puVar11 != (undefined2 *)0x0) {
      *puVar11 = 0x9c60;
      *(undefined1 *)(puVar11 + 1) = 0xb8;
      *(undefined1 **)((int)puVar11 + 3) = &LAB_1003a430;
      *(undefined2 *)((int)puVar11 + 7) = 0xd0ff;
      *(undefined1 *)((int)puVar11 + 9) = 0xa2;
      *(undefined **)(puVar11 + 5) = &DAT_104df624;
      *(undefined4 *)(puVar11 + 7) = 0x3d80619d;
      *(undefined **)(puVar11 + 9) = &DAT_104df624;
      *(undefined4 *)(puVar11 + 0xb) = 0x8b077400;
      *(undefined4 *)(puVar11 + 0xd) = 0xffce8b16;
      puVar11[0xf] = 0x5052;
      *(undefined1 *)(puVar11 + 0x10) = 0xc3;
      BVar9 = VirtualProtect(&DAT_00626352,7,0x40,&DStack_10);
      if (BVar9 != 0) {
        _DAT_00626352 = CONCAT31((int3)((int)puVar11 + -0x626357),0xe8);
        DAT_00626356 = (undefined1)((uint)((int)puVar11 + -0x626357) >> 0x18);
        DAT_00626357 = 0x90;
        DAT_00626358 = -0x70;
        VirtualProtect(&DAT_00626352,7,DStack_10,&DStack_10);
        SVar20 = 7;
        puVar19 = &DAT_00626352;
        pvVar10 = GetCurrentProcess();
        FlushInstructionCache(pvVar10,puVar19,SVar20);
        LogVariadic("[DOLLTHR] doll invalidate throttled to ~11Hz @0x626352");
      }
    }
  }
  if (DAT_104df1fc != '\0') {
    FUN_100308c0();
    (*DAT_104df0e4)(0x8d40,DAT_104df58c);
    (*DAT_104df4d8)(0,0,DAT_104df580,DAT_104df04c);
    if ((DAT_104df1fc != '\0') && (DAT_100942d8 != (byte *)0x0)) {
      if ((int)DAT_10092b98 < 0) {
        UVar12 = GetPrivateProfileIntA("IEex Options","UI Single Buffer",1,".\\Icewind2.ini");
        DAT_10092b98 = (uint)(UVar12 != 0);
      }
      if (DAT_10092b98 != 0) {
        *DAT_100942d8 = 1;
      }
    }
  }
  uVar18 = CONCAT44(DAT_103debf4,DAT_103debf0);
  if (DAT_104df0ed != '\0') {
    uVar18 = _Xtime_get_ticks();
    uVar18 = __alldiv(uVar18,10,0);
  }
  DAT_103debf4 = (undefined4)((ulonglong)uVar18 >> 0x20);
  DAT_103debf0 = (undefined4)uVar18;
  UVar12 = GetPrivateProfileIntA("IEex Options","Show FPS",0,".\\Icewind2.ini");
  iVar6 = DAT_10094550;
  if (((((UVar12 == 0) || (DAT_104df1fc == '\0')) || (DAT_104df1ff == '\0')) ||
      ((iVar2 = *DAT_100944b8, iVar2 == 0 || (*(int *)(iVar2 + 0x91c) == 0)))) ||
     ((*(int *)(iVar2 + 0x3c4) == 0 || (*(int *)(iVar2 + 0x3c4) == *(int *)(*DAT_10094384 + 0x1c88))
      ))) {
    DAT_104df577 = 0;
    uVar17 = 0;
    if (UVar12 != 0) {
      uVar17 = 1;
      goto LAB_1003aa3e;
    }
  }
  else {
    uVar17 = 1;
  }
  DAT_104df577 = uVar17;
  uVar17 = 0;
LAB_1003aa3e:
  *(undefined1 *)(*DAT_100944b8 + 0x1a2a) = uVar17;
  if (local_14 != iVar6) {
    bVar1 = *DAT_100942d8;
    DAT_10094534 = (uint)bVar1;
    uVar13 = FUN_1003a460();
    uVar14 = FUN_1003a460();
    LogVariadic("[Blank] %s -> %s: blanking %d frame(s)",uVar14,uVar13,(uint)bVar1);
  }
  DAT_10094550 = local_14;
  uVar15 = DAT_10094534 - 1;
  if (*(int *)(*DAT_100944b8 + 0x91c) == 0) {
    bVar3 = 0 < (int)DAT_10094534;
    DAT_10094534 = uVar15;
    if (bVar3) {
      FUN_10024b40(*(undefined4 *)(*(int *)(*DAT_100944b8 + 0x3c4) + 4));
    }
  }
  else {
    bVar3 = 0 < (int)DAT_10094534;
    DAT_10094534 = uVar15;
    if (bVar3) {
      FUN_10024f40();
      return;
    }
  }
  return;
}
```

## Notes

### Shared present / back-buffer machinery (all helpers in this group)

The DLL takes over the end of the engine frame: the staged patches reroute
SwapBuffers, the alt-tab handler, the cursor query, the save capture, the
Bink frame copy and the FPS-counter bracketing into fork code that
renders the game into its own framebuffer objects, composites them onto
the desktop with aspect-correct letterboxing, and optionally moves the
real present onto a dedicated thread.

#### Modes and ini switches

- HD / Fill-Screen mode: master flag `DAT_104df1fc`, FBO-completeness
  flag `DAT_104df1ff`. Set once by FUN_10030290 (below) when
  (`[IEex Options] Windowed` OR `[IEex Options] Fill Screen`, both
  defaulting on) and the game and desktop resolutions are all > 0 and
  the bulk GL resolver FUN_1002fff0 succeeded and the FBO is complete.
- Present thread: `[IEex Options] Present Thread` (default 1) stored in
  `DAT_104df504`; `[IEex Options] Single Present Owner` (default 0) in
  `DAT_104df505` (consulted only when Present Thread is on). Sync-forced
  fallback flag `DAT_103d47ed`.
- Pacing: `[IEex Options] Max FPS` (default 0 -> display refresh - 2;
  values < 1 take the fallback) in `DAT_104df6ac`; refresh from
  FUN_10024db0 (below); `[IEex Options] Present Hold Ms` (default 0,
  clamped to 2000) in `DAT_104df884`/`DAT_104df87c`; token wait
  `[IEex Options] Present Token Wait Ms` (default 50, clamped 1..5000)
  in `DAT_104df880`/`DAT_104df878`.
- Wine detection: `DAT_10092b84` (lazy: `ntdll!wine_get_version`
  resolvable -> 1, else 0; negative = not yet checked). Several paths
  (TLS swap-interval mirror, alt-tab interception) are no-ops on Wine.

#### Fill-Screen one-shot init (inner helper FUN_10030290)

Runs once (flag `DAT_100945f3`) at the first BlankBackBuffer. Reads
Windowed/Fill Screen, snapshots the game render resolution from the
engine screen words at `0x8ba31c`/`0x8ba31e` into `DAT_104df580` (w) /
`DAT_104df04c` (h) and the desktop resolution into `DAT_104df518` /
`DAT_104df3ac`, computes the fitted content size (`DAT_104df4f8` /
`DAT_103deff8`), letterbox offsets (`DAT_104df230` / `DAT_104df0c4`) and
fit scale (`DAT_10092750`, the min scale, double). Creates the FRONT
FBO/texture pair: texture `DAT_103ded98` (GL_TEXTURE_2D, GL_RGBA8
0x8058, GL_LINEAR 0x2601, GL_CLAMP_TO_EDGE 0x812F) attached as
GL_COLOR_ATTACHMENT0 (0x8CE0) to FBO `DAT_104df58c`; on
`glCheckFramebufferStatusEXT != GL_FRAMEBUFFER_COMPLETE` (0x8CD5) HD mode
is disabled ("Fill Screen: FBO incomplete, disabled"). On success it
restyles the game window (style `0x16ca0000`, EXSTYLE topmost bit
cleared), re-centers it (SetWindowPos NOZORDER|NOACTIVATE), calls
ClipCursor(NULL), reads Present Thread / Single Present Owner, logs
"Fill Screen ON: game %dx%d -> desktop %dx%d (fit %d,%d %dx%d)" and
"[Present] Fill Screen=1 Present Thread=%d Single Present Owner=%d
Windowed=%d". The BACK pair (FBO `DAT_104df25c`, texture
`DAT_104df0b4`) is created by FUN_10030790 with identical parameters
(genlTextures + glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, gameW, gameH,
0, GL_RGBA, GL_UNSIGNED_BYTE, 0), LINEAR/CLAMP_TO_EDGE,
glGenFramebuffersEXT + attach, viewport, clear black, unbind).

#### Present thread and shared context (inner helpers FUN_10031890 / FUN_10031500)

FUN_10031890 (called lazily from PresentScaled) creates three events -
work token `DAT_103dedc0` (auto-reset), frame release `DAT_104df520`
(auto-reset, initially signaled), thread-ready `DAT_104df00c`
(manual-reset) - and a worker GL context shared with the engine's: from
the engine HDC/HGLRC at `_DAT_008cf6d8+0x914`/`+0x918` it tries
wglCreateContextAttribsARB (`DAT_104df374`, attribs at `DAT_100672a0`)
and falls back to wglCreateContext + wglShareLists; failure logs
"[Present Thread] ctx create failed at %s (gle=%lu)" and returns 0.
Otherwise it spawns the thread (CreateThread -> FUN_10031500),
SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL), waits up to 3 s on
thread-ready, and logs "[Present Thread] up (fences %s)" or "OFF ->
glFinish handoff".

FUN_10031500 (thread proc): resolves wglMakeCurrent; gets the game HWND
(`DAT_104df0a8`, WindowFromDC) and a DC; `wglMakeCurrent(hDC,
DAT_104df528)` (the worker context) must succeed or it logs "[Present
Thread] init failed at %s (gle=%lu)" and the session stays synchronous.
On success it records the present HDC (`DAT_104df0d0`) and thread id
(`DAT_103ded9c`), sets the swap-interval state `DAT_10092768` and the
fork TLS slot +0x9c to the sentinel 0xFFFFFFFE, raises the up-flag
`DAT_103ded6f` and signals thread-ready. Loop: wait the work token;
while not sync-forced: wait the previous frame's fence
(`glClientWaitSync(DAT_103dedb0, 0, 200000000)` = 200 ms, then
glDeleteSync), accumulate fence time (`DAT_104df628`/`62c`); if the
hand-off flag `DAT_104df507` is clear, run FUN_10030d10 (which performs
the real SwapBuffers for the frame handed over by PresentScaled), else
clear the flag; accumulate present-thread time (`DAT_104df600`/`604`);
if `DAT_1009278f`, run FUN_10025280 (frame limiter); accumulate the
per-frame time (`DAT_103dedb8`/`bc`); SetEvent(frame release). Exit
path (context loss / shutdown): wglMakeCurrent(0,0),
wglDeleteContext(`DAT_104df528`), ReleaseDC.

#### Async hand-off and the wedge ladder (PresentScaled fast path)

When all of these hold - Bink did not just present, Present Thread ini
on, not sync-forced, HD mode, active screen == World
(`*(*DAT_100944b8 + 0x3c4) == *(*DAT_10094384 + 0x1c88)`), not a
dialog-idle frame (FUN_10037a20 == 0), thread up - PresentScaled polls
the frame release event with a 0 ms timeout (timing bracketed by the
us clock into `DAT_104df378`/`37c`):

- Signaled: drop counter reset; hand-off - stash the front texture
  (`DAT_104df5f8`) and the HDC (`DAT_103defd8`) for the worker, clear
  `DAT_104df507`, set `DAT_1009278f` (worker owns the real
  SwapBuffers); if glFenceSync (`DAT_104df050`) and glClientWaitSync
  (`DAT_104df20c`) exist, `DAT_103dedb0 = glFenceSync(0x9117, 0)`
  (GL_SYNC_GPU_COMMANDS_COMPLETE) and the flush proc is glFlush
  (`DAT_104df020`), else no fence and the flush proc is glFinish
  (`DAT_104df614`); call the flush; swap the FBO ids
  (`DAT_104df58c <-> DAT_104df25c`) and texture ids
  (`DAT_103ded98 <-> DAT_104df0b4`); FUN_10030a40 blits the old front
  content into the new front FBO so both sides see the latest frame;
  SetEvent(work token); done for this frame.
- Not signaled: drop counter `DAT_104df6b8`++; below 0x259 (601) it
  just returns (the frame is skipped); at 601 consecutive drops it sets
  the session sync-forced flag and logs "[Present Thread] wedged
  (continuous drops) -> sync present", falling through to the sync
  path permanently.

#### Sync composite (inner helper FUN_10030d10)

The synchronous path (also used by DialogIdlePresent and every
non-eligible frame): draws the current back texture into the front FBO
as a letterboxed quad over a black clear (glBlitFramebufferEXT
READ/DRAW blit back->front with GL_NEAREST first, then the letterbox
quad), applies Show-FPS (engine DisplayFrameRate `0x795c70` while the
engine's Show-FPS byte `+0x1a2a` is set) and the dialog-idle re-present
(one-shot `DAT_104df627`: gate FUN_100375b0 + FUN_10037a20, clear the
engine dirty counters at `+0xe4..0xf0`, draw the engine cursor via
`0x7be300` under the re-entrancy guard `DAT_100945f1`). Token protocol
when the present thread owns the swap: SetEvent(work token) after
fence/glFlush/glFinish, and the caller waits the frame release event
with the token-wait timeout (2 s backoff deadline `DAT_103dec20` after
failures; 5 consecutive token timeouts, counter `DAT_104df38c`, degrade
the session to synchronous). When the caller is not the owner it
performs the real SwapBuffers itself through the engine's IAT mirror
`_DAT_00a0e170`. Finally sleeps `[IEex Options] Present Hold Ms`.

#### Vsync state machine, watchdog and frame limiter (inner helpers FUN_10025280 / FUN_10024db0)

Swap-interval state `DAT_10092768`: 1 = vsync on, 0 = forced off,
0xFFFFFFFE = present-thread sentinel (the worker thread owns the
actual swap). Requested interval `DAT_100927ac` (writer outside this
seed set). PresentScaled mirrors the effective value to the CURRENT
thread's fork TLS slot +0x9c on Wine only, once per movie-boundary
frame (pending flag `DAT_103d47ec`). FUN_10025280 (run on frames
FUN_10030d10 did not absorb, and by the present thread) is a watchdog
with a 5 s reschedule (`DAT_104df720`/`724`): it measures 48
consecutive frame spacings (`DAT_104df384`); on a display reporting
> 100 Hz (refresh from FUN_10024db0, `DAT_104df74c`), ~15-18 ms
spacings while the interval is 1 indicate the driver locked the present
to ~60 fps - stage 1 forces interval 0 and retries (`DAT_104df5c4` = 1,
retry counter `DAT_104df0c0`, log "[VSYNC] interval-1 present locked to
~60 fps (display %d Hz, cap %d) -> forcing interval 0, will retry");
if the lock persists it is off for the session (`DAT_104df5c4` = 2, log
"[VSYNC] 60 Hz lock persisted after retry -> vsync off for this session
(frame limiter paces at %d fps)"). It also paces uncapped frames to
`[IEex Options] Max FPS` (refresh - 2 when unset). FUN_10024db0
resolves the display refresh: cached HWND (`DAT_10094554`, IsWindow
validated, else WindowFromDC of the engine HDC), MonitorFromWindow
(MONITOR_DEFAULTTONEAREST), GetMonitorInfoA, EnumDisplaySettingsA
(ENUM_CURRENT_SETTINGS; freq > 1 -> max(freq, FUN_10024bb0(dmPelsHeight))),
falling back to EnumDisplaySettingsA(NULL, ...), else a literal 120.

#### Context-loss recovery (inner helper FUN_100308c0)

Called at the top of BlankBackBuffer and DialogIdlePresent HD paths:
glIsTexture checks on the paired textures; if any is gone it stops the
present thread (event close, 200 ms wait, 1 s join), recreates the
events and thread (FUN_10031890), rebuilds the back pair (FUN_10030790)
and logs "[Present Thread] restarted after GL context loss" and
"Fill Screen: GL context lost -> FBO rebuilt".

#### Show-FPS ownership split

`[IEex Options] Show FPS` (default 0) is re-read EVERY frame by
BlankBackBuffer. When it is on but the fork cannot draw the counter
(HD off, FBO incomplete, no engine/UI manager, or a non-World screen),
the engine's own Show-FPS byte `*(*DAT_100944b8 + 0x1a2a)` is set so
the engine draws its counter; otherwise the byte is cleared and the
fork flag `DAT_104df577` makes the composite draw engine
DisplayFrameRate `0x795c70` itself.

#### Blank model and init-time hook installs (BlankBackBuffer)

The helper is the fork's per-frame init point as well as the blank
driver. One-time installs, each signature-gated and logged:

- GLINFO: GetProcAddress("glGetString"), logs "[GLINFO] vendor=%s |
  renderer=%s | version=%s" once (GL_VENDOR 0x1F00, GL_RENDERER 0x1F01,
  GL_VERSION 0x1F02; empty-string fallback `DAT_10065c64`; flag
  `DAT_104df768`).
- LBLFIX (FUN_10039fe0): ini `[Options] "Label Dirty Check"` in
  `.\IEex.ini` (default 1). Two hooks: CUIControlLabel::SetText at
  0x4E46F0 (6 bytes, sig 'V','h') -> FUN_10039ef0, log "[LBLFIX]
  CUIControlLabel::SetText dirty-check installed @0x4E46F0"; and the
  ~CUIControlLabel cache purge at 0x4E42A0 (7 bytes, sig 6A FF 68) ->
  FUN_1003a220, log "[LBLFIX] ~CUIControlLabel cache purge installed
  @0x4E42A0". Disabled: "[LBLFIX] DISABLED by IEex.ini [Options] \"Label
  Dirty Check\"=0 (A/B diag)". FUN_10039ef0 keeps an 8-slot x 0xA4 memo
  table at `DAT_103d47f0` ({dword key, char[0xA0] text}, probe
  `(i + (key >> 4)) & 0xff`): unchanged text -> return 1 and the hook
  SKIPS the original SetText; changed -> update + 0. FUN_1003a220
  clears the matching entry on destruction.
- SPELLICON #1 at 0x66D5A0 (7 bytes `6A FF 68 ..`, flag `DAT_104df6a8`)
  -> pushad trampoline -> FUN_1003a260, log "[SPELLICON]
  CUIControlButtonSpellbookSpell::SetSpell invalidate-on-change
  installed @0x66D5A0". FUN_1003a260 receives ECX (= the control this)
  and the function's first stack argument, FNV-1a-hashes the spell-name
  string, keeps a cache (sentinel `DAT_103d46c0`, 0x14-byte nodes via
  operator_new + FUN_1000dfe0/FUN_10013210), and calls the real
  `0x4d56a0` (CUIControlButtonSpellbookSpell::SetSpell) only when the
  id pair differs from the cached pair at this+0x666/0x66a. The
  trampoline re-executes the displaced prologue and jumps back to
  0x66D5A7 (hooksite+7; the decompiled rel32 is stored relative to the
  trampoline base - verified by hand).
- SPELLICON #2 at 0x66DC30 (same shape, flag `DAT_104df700`) ->
  FUN_100124e0: when the control's +0x66e flag is set, FUN_10012340()
  re-warms the icon cache; log "[SPELLICON]
  CUIControlButtonSpellbookSpell::Render icon-cache warm installed
  @0x66DC30"; jump-back target 0x66DC37.
- DOLLTHR at 0x626352 (7 bytes `8B 16 8B CE FF 52 50` =
  mov edx,[esi]; mov ecx,esi; call [edx+0x50], flag `DAT_104df755`) ->
  the 7 bytes become `call rel32` into a pushad/pushfd stub that calls
  FUN_1003a430, stores its byte result to `DAT_104df624`, restores
  flags/registers and - when allowed - re-executes the displaced
  virtual call inline before returning; throttled (byte == 0) it skips
  it. FUN_1003a430: `GetTickCount() - DAT_104df634 < 0x5a` (90 ms,
  ~11 Hz) -> 0 else stamp + 1. Log "[DOLLTHR] doll invalidate throttled
  to ~11Hz @0x626352".

Per-frame blank logic: on a change of the argument (the ECX value at
the hook site, an engine screen/back-buffer identity used only as the
change key) it sets the blank count to `*DAT_100942d8` (the engine
single-buffer byte) and logs "[Blank] %s -> %s: blanking %d frame(s)"
with screen names from FUN_1003a460 (World / SelectParty / CreateChar /
Chapter / Start / Load / Save / Options / Inventory / Journal / Map /
Spellbook / Store / Character / WorldMap / Movies / MultiPlayer /
Connection / Keys, resolved off the CScreen members at
`*DAT_10094384+0x1c58..0x1ca8`). Then ONE count decrement per call;
while positive it blanks - without a UI manager via FUN_10024b40 (a
handshake/callback drain over the active screen's +4 field), with one
via FUN_10024f40 (glClear(0,0,0,1) preserving scissor state).

#### Bink snapshot and present (RenderBinkFrameGL + inner helpers FUN_10039790 / FUN_10039440)

RenderBinkFrameGL snapshots each decoded movie frame into a CPU buffer
under a critical section (`DAT_103dec08`, lazy init, flag
`DAT_104df574`); FUN_10039790 (called first from PresentScaled) uploads
it (glTexImage2D GL_RGBA8 from buffer `DAT_104df500`, texture
`DAT_104df5f4`) and draws a letterboxed GL_QUADS frame whenever HD mode,
the Bink init flag and the active-movie gates hold, then FUN_10039440
draws the engine cursor over it (frame table `DAT_100927d0`, 0x1C-byte
entries, binary-searched by the movie frame counter `DAT_104df07c`).
While a movie presents, PresentScaled takes the Bink path and skips the
normal hand-off for that frame.

#### Save capture pair (ReadBufferForSave + ReadPixelsForSave)

The staged patches replace the two GL calls inside the engine's
screenshot path (CVidInf::PrintSurfaceToBmp). ReadBufferForSave selects
the read buffer; ReadPixelsForSave fetches the pixels - in HD mode via
glGetTexImage on the CURRENT front texture (`DAT_103ded98`) plus a
row-copy out of a full-frame scratch buffer, because the composite
lives in an FBO the default glReadPixels cannot see. Procs are seeded
from the ENGINE's IAT slots (glReadBuffer `_DAT_009078c4`, glReadPixels
`_DAT_009078c8`, glGetTexImage `_DAT_00907700`) with opengl32
GetProcAddress fallbacks, so the capture works even when called on a
thread where the fork's resolver has not run.

#### GL entry points used by this group

All GL calls go through proc slots resolved at init. Slot -> name for
this group (resolution sites verified; the bulk-resolver one-call-lag
artifact is documented in the hud notes):

| slot | proc | resolved by |
| --- | --- | --- |
| `DAT_104df52c` | glMatrixMode | FUN_100257e0 (one-shot, flag `DAT_104df740`) |
| `DAT_104df50c` | glLoadIdentity | FUN_100257e0 |
| `DAT_104df524` | glTranslatef | FUN_100257e0 |
| `DAT_104df56c` | glScalef | FUN_100257e0 |
| `DAT_103debfc` | glPushMatrix | FUN_100257e0 |
| `DAT_104df608` | glPopMatrix | FUN_100257e0 |
| `DAT_104df050` | glFenceSync | FUN_10031890 path (wglGetProcAddress) |
| `DAT_104df20c` | glClientWaitSync | FUN_10031890 path |
| `DAT_104df210` | glDeleteSync | FUN_10031890 path |
| `DAT_104df374` | wglCreateContextAttribsARB | FUN_10031890 path |
| `DAT_103defd0` | glBlitFramebufferEXT | FUN_10031890 path |
| `DAT_104df020` | glFlush | FUN_10031890 path (GetProcAddress) |
| `DAT_104df614` | glFinish | FUN_10031890 path (GetProcAddress) |
| `DAT_104df734` | glFinish (separate slot) | FUN_10037630 |
| `DAT_104df590` | wglSwapIntervalEXT | FUN_10025280 |
| `DAT_104df6f4` | glClear | FUN_10024f40 (one-shot) |
| `DAT_104df728` | glClearColor | FUN_10024f40 |
| `DAT_104df6e0` | glDisable | FUN_10024f40 |
| `DAT_104df714` | glEnable | FUN_10024f40 |
| `DAT_104df748` | glIsEnabled | FUN_10024f40 |
| `DAT_104df0e4` | glBindFramebufferEXT | bulk resolver FUN_1002fff0 |
| `DAT_104df3d8` | glFramebufferTexture2DEXT | bulk resolver |
| `DAT_104df5b0` | glCheckFramebufferStatusEXT | bulk resolver |
| `DAT_104df4d8` | glViewport | bulk resolver |
| `DAT_104df584` | glClearColor | bulk resolver |
| `DAT_104df610` | glClear | bulk resolver |
| `DAT_104df60c` | glMatrixMode | bulk resolver |
| `DAT_104df4e4` | glDisable | bulk resolver |
| `DAT_104df024` | glEnable | bulk resolver |
| `DAT_104df390` | glIsTexture | bulk resolver |
| `DAT_104df000` | glScissor | bulk resolver |
| `DAT_104df568` | glGetIntegerv | bulk resolver |
| `DAT_104df630` | glDeleteTextures | bulk resolver |
| `DAT_104df014` | glDeleteFramebuffersEXT | bulk resolver |
| `DAT_104df3a0` | glGenFramebuffersEXT | bulk resolver |
| `DAT_104df3a4` | glGenTextures | bulk resolver |
| `DAT_103dec04` | glBindTexture | bulk resolver |
| `DAT_103ded88` | glTexImage2D | bulk resolver |
| `DAT_104df0e0` | glTexParameteri | bulk resolver |
| `DAT_104df3e8` | glLoadIdentity | bulk resolver |
| `DAT_104df3c0` | glPushMatrix | bulk resolver |
| `DAT_104df620` | glPopMatrix | bulk resolver |
| `DAT_103debf8` | glOrtho | bulk resolver |
| `DAT_104df5c0` | glTexEnvf | bulk resolver |
| `DAT_103deffc` | glColor4f | bulk resolver |
| `DAT_104df5f0` | glTexCoord2f | bulk resolver |
| `DAT_104df4f0` | glVertex2f | bulk resolver |
| `DAT_104df220` | glBegin | bulk resolver |
| `DAT_104df3fc` | glEnd | bulk resolver |
| `DAT_103defe4` | glBlendFunc | bulk resolver |
| `DAT_104df718` | glReadBuffer | engine IAT 0x9078c4 (fallback GetProcAddress) |
| `DAT_104df70c` | glReadPixels | engine IAT 0x9078c8 (fallback GetProcAddress) |
| `DAT_104df6b0` | glGetTexImage | engine IAT 0x907700 (fallback GetProcAddress) |
| `DAT_104df76c` | glGetError | GetProcAddress in ReadPixelsForSave |
| `DAT_104df77c` | glBindTexture | GetProcAddress in ReadPixelsForSave |
| `DAT_104df6ec` | wglGetCurrentContext | GetProcAddress in ReadPixelsForSave |

FUN_100257e0 returns 1 only if all six of its procs resolved; FrameRate
bracketing and the DialogIdlePresent HD path gate on it.

#### Engine sites and VAs referenced by this group

| VA | meaning |
| --- | --- |
| 0x7BE520 | SwapBuffers call `FF 15 70 E1 A0 00` in CVidInf::WindowedFlip3d -> PresentScaled (6 bytes; hDC pushed at 0x7BE51F) |
| 0x7914D0 | CChitin::OnAltTab -> OnAltTabIntercept (5 bytes; stdcall (hWnd, bActivate), ret 8; return 1 = skip stock) |
| 0x8474D4 | USER32!GetCursorPos IAT slot -> GetCursorPosGated (staged Lua stub `!call_[dword] #8474D4`; engine sites 0x78F2B3 / 0x45F6F8 rewritten) |
| 0x7BE805 | `call ds:glReadBuffer` `FF 15 C4 78 90 00` in CVidInf::PrintSurfaceToBmp -> ReadBufferForSave (only glReadBuffer call site in the binary) |
| 0x7BE857 | `call ds:glReadPixels` `FF 15 C8 78 90 00` in PrintSurfaceToBmp -> ReadPixelsForSave |
| 0x43E300 | CBaldurProjector::Render replaced whole by RenderBinkFrameGL (`!marked_esp !push([esp+0x04]); !push_ecx; !call; !ret_word 04 00` = thiscall (this, HBINK) -> stdcall) |
| 0x79C1F8 | 6 bytes `8B 16 6A 00 8B CE` before `call 0x795c70` in CVidInf::Flip -> FrameRateBegin |
| 0x79C201 | 5 bytes `A1 D8 F6 8C 00` after it -> FrameRateEnd |
| 0x68DF53 | 7 bytes in CScreenWorld::TimerSynchronousUpdate render-block entry -> DialogIdlePresent (eax=1 -> jmp 0x68DFC3 skips the render block) |
| 0x790DC4 | 8 bytes -> BlankBackBuffer (`!push(ecx)` makes the ECX value the stack argument) |
| 0x795c70 | engine DisplayFrameRate (drawn by the composite under the +0x1a2a byte) |
| 0x7be300 | engine present-time cursor draw (dialog-idle re-present) |
| 0x790570 | engine alt-tab helper (called on the activate path) |
| 0x4d56a0 | CUIControlButtonSpellbookSpell::SetSpell (SPELLICON re-issue target) |
| `_DAT_00a0e170` | engine SwapBuffers IAT mirror (the real flip) |
| `_DAT_008cf6d8` | engine global: +0x914 HDC, +0x918 HGLRC |
| `_DAT_008cf6dc` | second engine global: +0x1c54 -> object with +0x447a cursor-visible flag |
| `*DAT_100944b8` | engine: +0x91c UI manager, +0x3c4 active screen, +0x1a2a Show-FPS byte, +0x1c4a active byte, +0x7d6 / +0x1986 gate bytes, +0xe4..0xf0 dirty counters |
| `*DAT_10094384` | pVidMode/game-state: +0x1c54 screen-mode table (current = *(+0x37e2 + *(byte*)(+0x37e0)*4)), +0x1c88 World, +0x1c58..0x1ca8 CScreen members |
| 0x8ba31c / 0x8ba31e | engine game-resolution words (16-bit) |
| `DAT_008fb950` | fork "in background" byte (alt-tab) |
| 0x9078c4 / 0x9078c8 / 0x907700 | engine IAT: glReadBuffer / glReadPixels / glGetTexImage |

#### Threads and synchronization

Two GL contexts and up to three threads: the game thread (renders into
the back FBO, runs BlankBackBuffer / PresentScaled / the hooks), the
present thread (owns the real SwapBuffers when Single Present Owner,
paces frames, waits fences), and the Lua thread (never touches this
group). Shared state: the Bink critical section `DAT_103dec08`
(RenderBinkFrameGL writer vs FUN_10039790/10039440 readers); the three
events (work token / frame release / thread-ready); the hand-off flags
`DAT_104df507` / `DAT_1009278f` and the wedge/sync flags
`DAT_103d47ed` / `DAT_103d47ec`; everything else (ini caches, letterbox
geometry, counters, memo tables) is unsynchronized bytes/dwords touched
from one thread each in practice. Timing accumulators are 64-bit
(`DAT_104df378:37c` present wait, `DAT_104df098:09c` total,
`DAT_104df600:604` present thread, `DAT_104df410:414` game thread,
`DAT_104df5e8:5ec` glFinish cost, `DAT_104df628:62c` fence wait,
`DAT_103dedb8:bc` present-thread frame, `DAT_103debf0:bf4` frame
stamp); the us clock is gated by availability flag `DAT_104df0ed` and
wraps at 2^32 us.

#### Logging

All log lines go through `DAT_100944fc` (log function) and
`DAT_100944f8` (LogVariadic sink) installed by the InitHelperDLL
export; a null sink drops them silently (see the bridge notes).

#### Registration

All ten helpers are registered in the Lua raw-address table built by
FUN_1000e2a0 (bridge group) and are reached only through machine-code
hooks patched by the staged Lua - none is a Lua-callable
ExposeFunctions shim. Callers per note (staged patches):
IEex_Gui_Patch.lua -> BlankBackBuffer, DialogIdlePresent;
IEex_UIScale_Patch.lua -> FrameRateBegin, FrameRateEnd;
IEex_Render_Patch.lua -> the rest; the cursor-site rewrites at
0x78F2B3/0x45F6F8 come from IEex_Key_Patch.lua.
`IEex_Helper_BlankBackBuffer` - 0x1003a590 - registered in the Lua
raw-address table; entered from the staged hook at 0x790DC4 (8 bytes,
`!push(ecx)` - the ECX value at that engine site becomes the single
stack argument; the helper itself is cdecl-style, caller balanced).

Signature: `void f(int screenKey)` - the argument is only used as the
change-detection key of the blank logic; the decompile never dereferences
it.

Behavior, in order (full detail in the shared sections above):

1. GLINFO one-shot log (glGetString VENDOR/RENDERER/VERSION).
2. FUN_10030290 - Fill-Screen one-shot init (geometry snapshot, front
   FBO/texture pair, window restyle/recenter, ClipCursor(NULL), ini
   reads, mode logs).
3. FUN_10039fe0 - LBLFIX installer (SetText dedup + dtor purge, ini
   `[Options] Label Dirty Check` in `.\IEex.ini`).
4. SPELLICON #1/#2 + DOLLTHR one-shot installs (signature-gated byte
   patches at 0x66D5A0, 0x66DC30, 0x626352 with pushad trampolines;
   logs as listed above).
5. HD block: FUN_100308c0 (context-loss recovery); glBindFramebufferEXT
   (GL_FRAMEBUFFER, front FBO `DAT_104df58c`); glViewport(0, 0, gameW,
   gameH); lazy `[IEex Options] UI Single Buffer` (cache
   `DAT_10092b98`) - when on, writes `*DAT_100942d8 = 1` (the engine's
   single-buffer byte).
6. Frame stamp `DAT_103debf0:bf4` = us clock (when available).
7. Show-FPS ownership split (ini re-read every frame; engine byte
   +0x1a2a vs fork flag `DAT_104df577`).
8. Blank logic: argument change -> count = `*DAT_100942d8`, log
   "[Blank] %s -> %s: blanking %d frame(s)"; one decrement per call;
   while positive, blank via FUN_10024b40 (no UI manager) or
   FUN_10024f40 (scissor-preserving glClear(0,0,0,1)).

Outputs: the installed hooks, the bound front FBO + game-res viewport
(HD frames render into it after this call), the blank clear, and the
Show-FPS/blank state changes.

##### Failure fallbacks

- Fill-Screen init incomplete (ini off, resolutions invalid, GL procs
  missing, FBO incomplete): `DAT_104df1fc`/`DAT_104df1ff` stay clear;
  steps 5's FBO bind is skipped entirely and every downstream helper
  takes its stock path - the engine renders exactly as vanilla.
- Hook installs are signature-gated: wrong bytes at 0x66D5A0 /
  0x66DC30 / 0x626352 -> no patch, no log, no crash.
- Blank path with a null UI manager uses the callback-drain variant so
  no CUIManager method is dereferenced.

##### Decompiler artifact

- The argument arrives via `local_c`/`local_14` (stack-cookie frame);
  one integer argument.
- The Show-FPS branch joins at `LAB_1003aa3e`; the net effect is the
  split described above (engine byte = 1 only when Show FPS is on and
  the fork cannot draw).
- The DOLLTHR signature is decompiled as the dword compare
  `_DAT_00626352 == -0x3174e975` (= bytes 8B 16 8B CE) plus the 0x52FF
  word and 'P' byte checks.
- The trampoline rel32 constants (`0x66d58c - (int)puVar8`) are stored
  relative to the trampoline base; the real jmp targets are hooksite+7
  (0x66D5A7 / 0x66DC37) - verified arithmetically, not a fork bug.

