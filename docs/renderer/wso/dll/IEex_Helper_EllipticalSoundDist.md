# IEex_Helper_EllipticalSoundDist

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100255f0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_100255f0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Sound_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x1002563c) */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_100255f0(int param_1,int param_2)

{
  longlong lVar1;
  short sVar2;
  undefined8 uVar3;

  sVar2 = _DAT_008ba31e;
  if (_DAT_008ba31e == 0) {
    sVar2 = 1;
  }
  uVar3 = __alldiv((longlong)(int)(uint)_DAT_008ba31c * (longlong)param_2,sVar2,0);
  lVar1 = __allmul(uVar3,uVar3);
  lVar1 = (longlong)param_1 * (longlong)param_1 + lVar1;
  if (lVar1 < 0x80000000) {
    return (int)lVar1;
  }
  return 0x7fffffff;
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

`IEex_Helper_EllipticalSoundDist` - 0x100255f0 (FUN_100255f0) -
stdcall, 2 stack args (dx, dy), ret 0x8 (both epilogues at objdump
raw 0x24a56 / 0x24a60), returns the squared distance in eax;
entered from the IEex_Sound_Patch.lua stubs at engine 0x7A9ED5
(CSound::Play distance test) and 0x7AA1FA (ResetVolume distance
test).

##### Behavior

Replaces the engine's inline circular distance test with an
aspect-stretched ellipse so the audible region matches the screen
rectangle at any aspect (patch comment: the vanilla circular
falloff over-covers vertically on wide screens):

1. `h = (short)*(0x8BA31E)` (SCREENHEIGHT); `if (h == 0) h = 1`
   (division guard).
2. `dy' = SCREENWIDTH * dy / h` -- 64-bit intermediate via the
   __alldiv / __allmul CRT helpers, so wide resolutions and large
   dy cannot wrap a 32-bit product.
3. Return `dx*dx + dy'*dy'`, saturating: `result < 0x80000000 ?
   (int)result : 0x7fffffff`.

Only the DY axis is stretched (multiplied by the live W/H aspect);
dx keeps the engine's native unit, so the vanilla radius semantics
hold on the horizontal axis and the ellipse grows taller relative
to it exactly as the window gets wider. The result feeds the
vanilla `cmp` against `m_nRange^2` unchanged -- the squared form
is why the helper can stay allocation-free and branch-light.

##### Failure fallbacks

None needed: height 0 -> 1, the 64-bit intermediates cannot
overflow for any real resolution and dy (the saturation catch is
defensive), and the engine statics are always readable. A caller
that somehow gets 0x7fffffff back simply fails the range compare
-- the sound is quiet at that distance, which is the correct
degradation.

##### Decompiler artifact

The decompile carries Ghidra's "WARNING: Removing unreachable block
(ram,0x1002563c)" -- the tail of the saturation path; the two `ret
$0x8` epilogues at raw 0x24a56 / 0x24a60 are the real epilogue.
The __alldiv / __allmul calls are the static CRT helpers, not
engine code.

##### Lua wiring

Play @0x7A9ED5 -- the vanilla inline math is 10 bytes (`mov
esi,edx; imul eax,eax; imul edx,esi; add eax,edx`) with eax = dy,
edx = dx on entry and ecx already holding `m_nRange^2` for the
compare at :7A9EDF. Stub: `8B F2` (mov esi,edx -- dx lands in the
callee-saved ESI the later pan divide expects), `51` (push ecx --
save m_nRange^2 across the call), `50` (push eax = dy, arg 2),
`56` (push edx = dx, arg 1 -- stdcall reads low-to-high, dx pushed
last), call EllipticalSoundDist, `59` (pop ecx), `!jmp_dword
:7A9EDF` -- eax now holds dist^2 and flows straight into the
vanilla compare. ResetVolume @0x7AA1FA -- entry eax = dy, edi =
dx; stub `50 57` (push eax, push edi), call, `!jmp_dword :7AA204`
(the cdq that starts the vanilla comparison); nothing needs
saving.

