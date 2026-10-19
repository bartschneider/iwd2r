# IEex_Helper_CSoundImp_DestructHook

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1003ab10` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1003ab10.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Sound_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1003ab10(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  undefined4 in_ECX;

  iVar1 = *DAT_10094398;
  iVar2 = *DAT_100944b8;
  EnterCriticalSection((LPCRITICAL_SECTION)(iVar2 + 0x394));
  iVar3 = (*DAT_100941c8)();
  if (iVar3 != 0) {
    (*DAT_10094314)(iVar3);
  }
  iVar3 = (*DAT_100941c8)();
  if (iVar3 != 0) {
    (*DAT_10094314)(iVar3);
  }
  iVar3 = 0;
  if (-1 < *(int *)(iVar1 + 0xd8)) {
    do {
      iVar4 = (*DAT_100941c8)(in_ECX,0);
      if (iVar4 != 0) {
        (*DAT_10094314)(iVar4);
      }
      iVar3 = iVar3 + 1;
    } while (iVar3 <= *(int *)(iVar1 + 0xd8));
  }
  LeaveCriticalSection((LPCRITICAL_SECTION)(iVar2 + 0x394));
  return;
}
```

## Notes

### Shared sound machinery (all helpers in this group)

The engine's 2D audio was tuned for 800x600: every sound is built
with a hardcoded audible radius of 768, the play-time distance test
is a plain circle, party-PC voices play at full volume regardless
of position, soundset folder names are stored localized, and the
sound destructor trusts mixer state the constructor never
initialized. The staged `IEex_Sound_Patch.lua` (with one paired
note in `IEex_Gui_State.lua`) fixes all five with stubs that funnel
through these helpers (decompiled from the fork's
`IEexHelper.fork.dll`).

#### The engine objects and fields

All engine addresses in this group come from the staged patch
comments and this DLL's disassembly -- untrusted, attributed, never
verified against the engine binary in this stream.

- `CSound` (one per playing sound): `m_nRange` at +0x20 -- the
  "audible radius" whose square the distance test compares
  against; `m_nArea` at +0x60 -- the area the sound belongs to
  (positional Play bails when it is 0 or differs from the mixer's
  active area); a pan-range field the Gui_State patch scales
  separately.
- `CSoundMixer` (reached through the fork BSS slots): +0x4
  pDirectSound (the patch comment: never initialized by the engine
  constructor), +0x58 the channel-slot array pointer, +0x68 and
  +0xA0 two list heads, +0xD8 nMaxChannelSlot (vanilla starts it
  uninitialized -- the patch forces -1, see the destruct hook),
  +0xF0 m_nActiveArea.
- Engine functions the patch names: CSoundImp::Construct 0x7A8BB0,
  the CSoundImp destructor 0x7A8E20, (positional) Play 0x7A9DB0,
  ResetVolume 0x7AA110, CGameSprite::VerbalConstant 0x702900,
  CGameSprite::PlaySound 0x7011E0, CDimm::AddToDirectoryList
  0x782240.

#### The engine statics and the fork's zoom float

- `0x8BA31C` / `0x8BA31E` -- the engine's SCREENWIDTH / SCREENHEIGHT
  ushorts, read directly by the DLL (the displacement sits below
  the image base, the same class as the markers group's engine
  palette). The Gui_State.lua comment pins them: "SCREENWIDTH
  @0x8BA31C is still the 800 default". ScaleSoundRange floors the
  width at 800 (the vanilla default, so a stale or tiny value
  cannot collapse audibility); EllipticalSoundDist guards a zero
  height to 1.
- `DAT_1008f298` -- the fork's animated current camera zoom (the
  committed zoom-group notes: ZoomWorldBegin lerps it; the staged
  patches call it g_fCameraZoom). Both scale helpers clamp it to
  max(zoom, 1.0) so zooming IN never shrinks the audible radius;
  zooming out grows it, matching the larger visible area.

#### INI keys

Both scale helpers read `GetPrivateProfileIntA("IEex Options", key,
default, ".\Icewind2.ini")` fresh on every call -- no cache, no
sentinel (unlike the marker group's lazy caches), so a settings
change lands on the next sound construction. This is file I/O on
the sound-construction path; the patch accepted that cost for
simplicity.

| Helper | INI key | Default | Clamp |
|---|---|---|---|
| ScaleSoundRange | SFX Audible Percent | 60 | [5, 300] |
| ScaleVoiceRange | Voice Audible Percent | 120 | [5, 300], then <= 4600 |

Both compute the same thing: `m_nRange = round(width * pct /
(zoom * 100.0))` with width = max(SCREENWIDTH, 800). At the
defaults on an 800-wide zoom-1 screen SFX get 480 (vanilla 768);
at 1920 wide, 1152; at 4K, 2304.

#### Threads and synchronization

All five run on engine threads: the Construct / VerbalConstant /
PlaySound sites on the game thread's sound construction and
playback paths, the destructor hook wherever CSoundImp objects die,
the Play / ResetVolume distance math on the sound update path. Only
the destruct hook takes a lock -- the fork's critical section at
`(*DAT_100944b8) + 0x394` (the UI/input root object; a new offset
alongside the +0x91c / +0x924 / +0x3c4 the other groups use) while
it sweeps the mixer's lists. The range and ellipse math is
lock-free: read-only engine statics, one fork float, one store into
the sound being constructed.

#### Failure model and engine-address attribution

Every path degrades toward vanilla: missing INI keys fall to the
60 / 120 defaults and out-of-range values clamp into [5, 300]; a
soundset folder that already exists leaves FixSoundSetFolder
untouched and a foreign folder with no candidate remap fails
exactly as vanilla (silently); the destruct hook skips any list
whose getter returns 0 and skips the whole slot loop when
nMaxChannelSlot is negative. EllipticalSoundDist cannot fail (its
only guards are the height-zero division guard and the 0x7fffffff
saturation). The `DAT_10094xxx` engine pointers are
runtime-registered BSS slots filled through the loader's public
address-list mechanism (Bubb13/IEexLoader, IEexDLL/IEexLua.asm:
IEex_Init / IEex_AddressList) -- init-order assumptions shared by
every group, guaranteed here because the Lua patch installs these
hooks only after the fork's exports are up.

`IEex_Helper_CSoundImp_DestructHook` - 0x1003ab10 (FUN_1003ab10) -
ECX = the destructing CSoundImp, no stack args, plain ret; entered
from the IEex_Sound_Patch.lua `IEex_HookRestore(0x7A8E20, 0, 7,
stub)` wrapper on the engine's CSoundImp destructor (the patch
comment names the site "CSoundImp::Export_DestructHook()").

##### Behavior

The patch comment states the purpose: "Fix crash involving sound
system not properly clearing invalid sounds from various internal
lists when they are destructed". The helper runs first inside the
hook wrapper (all registers preserved by
push_all/pop_all_registers_iwd2, ECX = the destructor's this), then
the original 7 restored bytes run:

1. `mixer = *DAT_10094398` (the CSoundMixer, a runtime-registered
   BSS slot), `root = *DAT_100944b8`; EnterCriticalSection on
   `root + 0x394` via IAT `*0x1005e0a8` (Leave via `*0x1005e0ac`)
   -- the fork's UI/input critical section (COMMON section).
2. Two registered fork-internal function pointers do the list
   work: `getter = *DAT_100941c8`, `remover = *DAT_10094314`,
   both self-cleaning through the pointer (no `add esp` at any
   call site). Calling form: ECX = the list head, stack args
   (the CSound*, 0). The getter returns the list node holding that
   sound, or 0; the remover unlinks (and presumably frees) it.
3. Sweep the two list heads: with ECX = mixer+0xA0, loop
   `while ((node = getter()) != 0) remover(node)`; repeat with
   ECX = mixer+0x68.
4. Channel slots: `if (*(int *)(mixer + 0xD8) >= 0)` --
   `slots = *(void **)(mixer + 0x58)`; for i in
   0..*(int *)(mixer + 0xD8) inclusive: `slot = slots[i]`, then
   the same getter/remover pair with ECX = slot + 0x10. When
   nMaxChannelSlot is -1 (the state the patch's 0x7AAE62
   HookReturnNOPs enforces at mixer start) the loop body never
   runs.
5. LeaveCriticalSection; plain ret.

Net effect: a destructing sound can no longer survive as a stale
entry in either mixer list or in any channel slot's list, which is
what the crash bullet describes.

##### Failure fallbacks

Getter returning 0 -> that list is clean, move on. nMaxChannelSlot
< 0 -> the whole slot loop is skipped (the patch's own comment:
"-1 so ... Export_DestructHook() doesn't crash when icewind2.ini's
[Alias] fields aren't properly updated" -- the slot array contents
depend on [Alias] configuration). The critical section is released
on every path; the sweep body allocates nothing and cannot throw.
The mixer and function-pointer slots are the usual init-order
assumptions (COMMON section), satisfied because the Lua patch
installs this hook only after the fork is up.

##### Decompiler artifact

The Ghidra decompile hides the calling conventions entirely: it
renders the calls as `(*DAT_100941c8)()` and
`(*DAT_10094314)(iVar3)` with no ECX or stack arguments, and
flattens the two list sweeps. The ECX = list-head / stack-args
(this, 0) decode, the mixer+0xA0 / +0x68 / slot+0x10 heads, and
the loop head at raw 0x39f81-0x39f89 (which re-pushes 0 and this
each iteration) are from the objdump at raw 0x39f10-0x39fbd.

##### Lua wiring

`IEex_HookRestore(0x7A8E20, 0, 7, [[ !push_all_registers_iwd2
!call >IEex_Helper_CSoundImp_DestructHook !pop_all_registers_iwd2
]])` -- the loader restores the original 7 bytes before calling
the stub body, so the destructor's own prologue runs unchanged
after the sweep. Paired engine fixes in the same Lua file (not
helpers): a hook at 0x7AAD80 writing `mov [ecx+0x4],0` --
CSoundMixer::pDirectSound (+0x4) is never initialized by the
engine constructor and the first release crashes at launch; and
HookReturnNOPs at 0x7AAE62 NOPing out `mov [esi+0xD8],-1` -- the
comment inverts it: vanilla DOES start nMaxChannelSlot
uninitialized, and the NOPs make the -1 start explicit so this
helper's slot loop is safe until the engine sets a real count.

