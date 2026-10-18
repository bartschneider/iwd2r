# IEex_StartupFixes_State.lua — fresh-install INI sanitisation

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_StartupFixes_State.lua`.
Fork-authored convenience fixes that run once at startup
(`IEex_AbsoluteOnce("IEex_StartupFixes_Once", ...)`). Not renderer work; it
is in the drop because a broken `icewind2.ini` prevents the game from
reaching a renderable state.

## Intent

Three fresh-install `icewind2.ini` repairs, all done through the profile
API from Lua (no code bytes patched):

1. Re-point the `[Alias]` HD0:/CD1:/CD2: paths at the real game directory
   (GOG installs ship wrong ones). CD2 is only written if the directory
   exists.
2. Pre-seed `[Movies] INTRO/MIDDLE/END = 1` so the intro movie is skippable
   on the very first launch. The comment spells out the engine bug:
   `CBaldurProjector::PlayMovieInternal` reads the in-memory skip flag
   (`field_145`) before `AddPlayedMovie` writes the `[Movies]` flag during
   that same first play and never re-reads it, so a fresh install cannot
   skip INTRO until the second launch.
3. Clamp garbage `[Program Options] Screen Position X/Y` (GOG inis ship
   junk like 22063544) to 0 when outside 0..32767, so the software-renderer
   window cannot start off-screen.

## Hooks table

None — the file patches no code. The engine side it touches through the INI
only:

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| — | (no hook sites; INI writes only) | — | — | — |

Named-but-unresolved engine references made in comments only:
`CBaldurProjector::PlayMovieInternal` and `AddPlayedMovie` are discussed but
not hooked and not address-cited; both are `unresolved in our tree` (no
address given to resolve).

## Data flow

State lives entirely in `icewind2.ini` (and the engine's own INI cache).
Read path: `IEex_Helper_GetGameDirectory()` → build the ini path →
`IEex_GetPrivateProfileInt` for the screen-position sanity check →
`IEex_WritePrivateProfileString/Int` for every fix. Nothing per-frame reads
any of it except the engine's normal INI handling (alias resolution at
startup, movie skip flags at play time, window position at
software-renderer creation).

## IEex_Helper_* entry points

- `IEex_Helper_GetGameDirectory()` — returns the running game's directory
  (the would-be HD0: alias target); used to build the `icewind2.ini` path.
- `IEex_Helper_DirectoryExists(path)` — boolean directory test, gates the
  CD1:/CD2: alias writes.

(The `IEex_WritePrivateProfileString/Int` and `IEex_GetPrivateProfileInt`
names are core-DLL INI API, not `IEex_Helper_*`; still relevant to B1 since
they are DLL exports the Lua side calls.)
