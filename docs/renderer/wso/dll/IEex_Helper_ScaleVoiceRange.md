# IEex_Helper_ScaleVoiceRange

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10025550` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10025550.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Sound_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_10025550(int param_1)

{
  ushort uVar1;
  UINT UVar2;
  int iVar3;
  float fVar4;

  UVar2 = GetPrivateProfileIntA("IEex Options","Voice Audible Percent",0x78,".\\Icewind2.ini");
  if ((int)UVar2 < 5) {
    UVar2 = 5;
  }
  else if (300 < (int)UVar2) {
    UVar2 = 300;
  }
  fVar4 = 1.0;
  if (1.0 <= DAT_1008f298) {
    fVar4 = DAT_1008f298;
  }
  uVar1 = _DAT_008ba31c;
  if (_DAT_008ba31c < 800) {
    uVar1 = 800;
  }
  iVar3 = (int)(((float)uVar1 * (float)(int)UVar2) / (fVar4 * 100.0) + 0.5);
  if (0x11f8 < iVar3) {
    iVar3 = 0x11f8;
  }
  *(int *)(param_1 + 0x20) = iVar3;
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

`IEex_Helper_ScaleVoiceRange` - 0x10025550 (FUN_10025550) -
stdcall, 1 stack arg (the CSound* under construction), ret 0x4
(objdump raw 0x249de); entered from the IEex_Sound_Patch.lua voice
stubs at engine 0x702C37 (VerbalConstant) and the three
CGameSprite::PlaySound sites 0x70238F / 0x70256B / 0x7027BE.

##### Behavior

Same formula as ScaleSoundRange (COMMON section) with the voice
ini key, a wider default, and one extra clamp:

1. `pct = GetPrivateProfileIntA("IEex Options", "Voice Audible
   Percent", 120, ".\Icewind2.ini")`, clamped into [5, 300].
2. Same width / zoom computation -> candidate range.
3. `if (candidate > 0x11f8) candidate = 0x11f8;` -- hard cap at
   4600, then the `m_nRange` store.

The cap exists because of an int32 overflow in the engine's volume
formula (patch comment): `m_nRangeVolume = 100 * (m_nRange^2 -
dist) / m_nRange^2`. 100 * m_nRange^2 wraps NEGATIVE once
m_nRange > ~4633 (sqrt(2^31 / 100)), so an over-large range makes
the voice silent at EVERY distance -- the comment records that an
earlier build with a fixed 7680 range did exactly that and muted
all voices. 4600 keeps the product under the wrap. At 4K / zoom 1
the clamp actually binds (3840 * 1.2 = 4608 -> 4600), and the
patch's target holds: a PC half a screen away stays ~83% audible
(100 * (1 - 1920^2/4600^2)). The 120 default (vs SFX 60) is what
buys that headroom.

The other half of the voice fix is routing: vanilla plays party-PC
voices through the GLOBAL full-volume Play overload while NPCs play
positionally, so PCs never fade with distance. The Lua stubs below
reroute those PC paths through positional Play (0x7A9DB0); this
helper then gives the constructed sound the wider, overflow-capped
range that makes the rerouted playback audible across a wide
viewport.

##### Failure fallbacks

As ScaleSoundRange, plus: the 4600 clamp is unconditional, so even
a hand-edited ini at 300% cannot reach the overflow region. The
rerouted positional Play validates `m_nArea` before playing (bails
`ret 0x10` at 0x7A9DCF when the sound's area is 0 and at 0x7A9DDA
when it differs from the mixer's m_nActiveArea +0xF0) -- the
PlaySound stubs (Lua wiring) fill the field precisely so those
checks pass.

##### Decompiler artifact

None significant; identical shape to ScaleSoundRange plus the
0x11f8 clamp.

##### Lua wiring

Two reroutes, both in IEex_Sound_Patch.lua:

1. VerbalConstant @0x702C37 -- replaces 11 bytes: `6A 00` (push 0
   = bReplay), `8D 4C 24 34` (lea ecx,[esp+0x34] = &cSound), `E8`
   call 0x7A9B10 (the global Play). Stub: `8D 44 24 30` (lea
   eax,[esp+0x30] = &cSound at the stub's shifted stack), `56`
   (save the speaking sprite in ESI), `50` (push eax = the
   helper's stdcall arg), call ScaleVoiceRange, `5E`, then the NPC
   positional-Play sequence reproduced verbatim: `8B 46 0A` (y),
   `8B 4E 06` (x), `6A 00 6A 00 50 51`, `8D 4C 24 40`, `68 42 2C
   70 00` (push return address 0x702C42), `!jmp_dword :7A9DB0` --
   positional Play's `ret 0x10` lands exactly where the global
   Play would have returned (`test al,al`).
2. Three sites in CGameSprite::PlaySound (patch comment; function
   at 0x7011E0) via the patch's own
   `IEex_HookPCVoicePositional(first, second)` helper -- installs
   a shared stub at `first`, `second` being the resume/return
   address: 0x70238F -> 0x702398, 0x70256B -> 0x702574,
   0x7027BE -> 0x7027C7. Stub: `8B 56 12` (mov edx,[esi+0x12] =
   the sprite's area), `89 57 60` (mov [edi+0x60],edx =
   cSound.m_nArea -- fills the field positional Play checks),
   `56 57 57` (save ESI, save EDI, push EDI = &cSound as the
   helper arg), call ScaleVoiceRange, `5F 5E`, then
   Play(x, y, 0, 0) with this=edi, the site's return address
   pushed, `!jmp_dword :7A9DB0`.

