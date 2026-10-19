# IEex_Helper_FixSoundSetFolder

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10025670` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10025670.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Sound_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_10025670(int param_1)

{
  char *_Dst;
  DWORD DVar1;
  HANDLE hFindFile;
  HANDLE hFindFile_00;
  BOOL BVar2;
  undefined1 auStack_4b4 [4];
  _WIN32_FIND_DATAA local_4b0;
  _WIN32_FIND_DATAA local_370;
  undefined4 local_22c;
  undefined4 local_228;
  undefined1 local_224;
  CHAR local_220 [264];
  CHAR local_118 [268];
  uint local_c;

  local_c = DAT_1008f040 ^ (uint)auStack_4b4;
  _Dst = (char *)(param_1 + 0x725a);
  if (*(char *)(param_1 + 0x725a) != '\0') {
    local_22c = *(undefined4 *)(param_1 + 0x7252);
    local_228 = *(undefined4 *)(param_1 + 0x7256);
    local_224 = 0;
    if ((char)local_22c != '\0') {
      FUN_1003bc70(local_220,0xffffffff,".\\sounds\\%.32s",_Dst);
      DVar1 = GetFileAttributesA(local_220);
      if (((DVar1 == 0xffffffff) || ((DVar1 & 0x10) == 0)) &&
         (hFindFile = FindFirstFileA(".\\sounds\\*",&local_4b0), hFindFile != (HANDLE)0xffffffff)) {
        do {
          if ((((byte)local_4b0.dwFileAttributes & 0x10) != 0) && (local_4b0.cFileName[0] != '.')) {
            FUN_1003bc70(local_118,0xffffffff,".\\sounds\\%s\\%s*.wav",local_4b0.cFileName,
                         &local_22c);
            hFindFile_00 = FindFirstFileA(local_118,&local_370);
            if (hFindFile_00 != (HANDLE)0xffffffff) {
              FindClose(hFindFile_00);
              LogVariadic("[SOUND] soundset folder remap \'%.32s\' -> \'%s\' (prefix \'%s\')",_Dst,
                          local_4b0.cFileName,&local_22c);
              _Dst[0] = '\0';
              _Dst[1] = '\0';
              _Dst[2] = '\0';
              _Dst[3] = '\0';
              *(undefined4 *)(param_1 + 0x725e) = 0;
              *(undefined4 *)(param_1 + 0x7262) = 0;
              *(undefined4 *)(param_1 + 0x7266) = 0;
              *(undefined4 *)(param_1 + 0x726a) = 0;
              *(undefined4 *)(param_1 + 0x726e) = 0;
              *(undefined4 *)(param_1 + 0x7272) = 0;
              *(undefined4 *)(param_1 + 0x7276) = 0;
              strncpy_s(_Dst,0x20,local_4b0.cFileName,0xffffffff);
              break;
            }
          }
          BVar2 = FindNextFileA(hFindFile,&local_4b0);
        } while (BVar2 != 0);
        FindClose(hFindFile);
      }
    }
  }
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

`IEex_Helper_FixSoundSetFolder` - 0x10025670 (FUN_10025670) -
stdcall, 1 stack arg (the CGameSprite* being loaded), ret 0x4
(objdump raw 0x24bd3); entered from the IEex_Sound_Patch.lua stubs
at engine 0x70CB48 (CGameSprite::Unmarshal) and 0x5A17E5
(CInfGame::ImportCharacter).

##### Behavior

Cross-language soundset folder remap. The problem (patch comment):
saves and .CHR files store each PC's voice set as the localized
SOUND FOLDER NAME ("FEMALE_BARD_1" on an English install,
"Femme_Barde_1" on French, ...), while the audio file prefix is
language-independent. Load an English save on a French install and
CDimm::AddToDirectoryList (0x782240, non-recursive) finds no such
folder -> none of the set's WAVs enter the sound key table -> EVERY
soundset line fails CSound::Play silently: no audio, no talking
portrait frame, no m_talkingCounter, the CHARSTR.2DA subtitle row
never reached.

The sprite fields: +0x7252 / +0x7256 hold the 8-byte file prefix
(m_secondarySounds -- language-independent), +0x725A the char[32]
soundset folder (what Unmarshal's rep-movs just copied from
CSavedGamePartyCreature.field_25E).

1. Folder empty -> return, untouched.
2. Copy the prefix into a 9-byte local with an explicit NUL; if
   `prefix[0] == 0` there is nothing to match on -> return.
3. `sprintf(buf, ".\sounds\%.32s", prefix)` and stat it with
   GetFileAttributesA; a real directory (attr != 0xffffffff and
   the 0x10 DIRECTORY bit set) -> return untouched (the normal
   same-language case; one stat of overhead).
4. Missing or not a directory -> scan `".\sounds\*"` with
   FindFirstFileA / FindNextFile; for each entry that is a
   directory (0x10 set) and not a dot-name (`cFileName[0] !=
   '.'`), search `".\sounds\%s\%s*.wav"` (candidate
   folder, prefix).
5. First hit: FindClose on both handles, log via LogVariadic:
   "[SOUND] soundset folder remap '%.32s' -> '%s' (prefix '%s')",
   zero all 32 folder bytes, `strncpy_s(folder, 0x20, candidate,
   _TRUNCATE)`, stop scanning.
6. No candidate -> the sprite is left exactly as loaded and the
   engine fails exactly as vanilla.

/GS: the ~0x4b4-byte frame carries the stack cookie
(`DAT_1008f040 ^` prologue, __security_check_cookie epilogue) over
the sprintf / strncpy_s buffers.

##### Failure fallbacks

Every failure mode leaves the sprite untouched: empty folder, empty
prefix, folder already valid, no candidate directory, FindFirstFile
failing mid-scan. Vanilla's silent failure is the floor; the helper
only ever improves on it. The remap is logged, so a support
conversation can see it happened.

##### Decompiler artifact

None significant beyond the /GS cookie noise: the big stack frame
and the `auStack_4b4` naming are the cookie-protected buffers, not
state.

##### Lua wiring

Two stubs, same shape: `!push_all_registers_iwd2` / push the sprite
(EBP at both sites) / `!call >IEex_Helper_FixSoundSetFolder` /
`!pop_all_registers_iwd2` / displaced bytes / jmp to the resume
address. (a) CGameSprite::Unmarshal @0x70CB48 -- displaced 6 bytes
`8B 0D DC F6 8C 00` (mov ecx,[0x8CF6DC] = g_pBaldurChitin), a
5-byte jmp plus 1 NOP, resume :70CB4E; placed after the rep-movs
that fill +0x725A and before the GetDirSounds path build, so the
remap happens before the engine enumerates the folder. (b)
CInfGame::ImportCharacter @0x5A17E5 -- displaced 5 bytes `A1 DC F6
8C 00` (mov eax,[0x8CF6DC]), after the memcpy from
pChrData+0x184; the site is the target of the `jae` at 0x5A17DB
(the folder-was-copied branch), resume :5A17EA -- covering .CHR
imports as well as saves.

