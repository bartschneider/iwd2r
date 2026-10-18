# IEex_Helper_ScaleSoundRange

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100254c0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_100254c0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_State.lua`, `IEex_Sound_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_100254c0(int param_1)

{
  ushort uVar1;
  UINT UVar2;
  float fVar3;

  UVar2 = GetPrivateProfileIntA("IEex Options","SFX Audible Percent",0x3c,".\\Icewind2.ini");
  if ((int)UVar2 < 5) {
    UVar2 = 5;
  }
  else if (300 < (int)UVar2) {
    UVar2 = 300;
  }
  fVar3 = 1.0;
  if (1.0 <= DAT_1008f298) {
    fVar3 = DAT_1008f298;
  }
  uVar1 = _DAT_008ba31c;
  if (_DAT_008ba31c < 800) {
    uVar1 = 800;
  }
  *(int *)(param_1 + 0x20) = (int)(((float)uVar1 * (float)(int)UVar2) / (fVar3 * 100.0) + 0.5);
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

`IEex_Helper_ScaleSoundRange` - 0x100254c0 (FUN_100254c0) -
stdcall, 1 stack arg (the CSound* under construction), ret 0x4
(objdump raw 0x24944); entered from the IEex_Sound_Patch.lua stub
replacing the 7-byte `mov [esi+0x20],0x300` store at engine
0x7A8C57 inside CSoundImp::Construct (0x7A8BB0).

##### Behavior

Replaces the engine's hardcoded construction-time sound range
(vanilla writes a flat 768 = 0x300 into `m_nRange`) with a
percentage of the visible screen width, adjusted for camera zoom
(COMMON section):

1. `pct = GetPrivateProfileIntA("IEex Options", "SFX Audible
   Percent", 60, ".\Icewind2.ini")`, clamped into [5, 300].
2. `zoom = max(DAT_1008f298, 1.0)`.
3. `width = max((ushort)*(0x8BA31C), 800)`.
4. `*(int *)(param_1 + 0x20) = (int)(width * pct / (zoom * 100.0)
   + 0.5)` -- all float math; the +0.5 makes the store
   round-to-nearest.

Per-construction and zoom-aware: a sound built while zoomed in
gets the smaller radius it deserves on screen, and re-constructions
(as a sound is re-triggered) pick up ini edits immediately. The
patch comment notes ambient sounds that want a fixed radius can
still override afterwards through the engine's own CSound::SetRange
-- the helper only replaces the default.

##### Failure fallbacks

Missing INI key -> the 60 default (deliberately non-vanilla; this
feature has no off switch in the ini). Out-of-range values clamp
into [5, 300]. The width static and zoom float are always readable,
so there is no other failure path. The helper cannot skip the
store -- the original store instruction is gone, replaced by the
jump to this stub; that is the design.

##### Decompiler artifact

None significant: one straight-line block, decompile matches the
objdump.

##### Lua wiring

Stub at 0x7A8C57: `!push_all_registers_iwd2` / `56` (`push esi` --
the CSound* this that Construct keeps in ESI, becoming the
helper's stack arg) / `!call >IEex_Helper_ScaleSoundRange` /
`!pop_all_registers_iwd2` / `!jmp_dword :7A8C5E` -- resume just
past the replaced store, all other registers untouched. Paired
note in IEex_Gui_State.lua (line 2932): "do NOT also patch the
0x7A8C57 store here: that region is now a jmp to the range stub" --
Gui_State's own construct-time pan work (SetPanRange arg scaled by
nWidth/800, stored at 0x42629E when nWidth > 800; maxNonInstantRange
-> 0x484BA2; HookRestore(0x6902D4, 3, 2, mov_ecx)) touches
neighbouring values in the same function but must leave this store
alone. The write primitives are the loader's public
IEex_WriteAssembly / IEex_HookRestore (IEexDLL/IEexLua.asm).

