# ADR 0001: Platform layer

Status: accepted

## Context

The upstream tree is Win32 x86 code built with MSVC: MFC (`_AFXDLL`), DirectDraw, DirectSound, DirectPlay and an OpenGL 3D mode. iwd2r must build natively on Linux and Windows from the start, and project 2 compares our build against the original 32-bit binary as an oracle. The decompiled function bodies carry address comments linking them to the original, so keeping them textually close to the binary has real value. Ubuntu 24.04 packages SDL2 only.

## Decision

- Shim the APIs and keep the code. `compat/` provides subset implementations of the MFC, Win32 and DirectX names with the same semantics; `platform/` declares the interfaces with no SDL or miniaudio types exposed.
- Window and input use SDL3, fetched by CMake at a pinned release tag. SDL3 is fetched because Ubuntu 24.04 packages only SDL2.
- Audio uses miniaudio, also fetched and pinned.
- The window is Win32-shaped: a message queue over SDL events feeds the same handler entry points, so `CChitin::WndProc` stays intact.
- The default build is 64-bit. A 32-bit Windows preset (`windows-msvc-x86`) exists so project 2 can compare struct layouts against the 32-bit oracle.
- Pointer-width policy for the engine's 32-bit casts: ids read back from pointer-sized storage go through `INT_PTR`, pointer keys are widened to `DWORD_PTR`, and packed and on-disk structs are left untouched.

## Alternatives considered

- Rewrite call sites to a native API. Rejected: it touches hundreds of sites in 266,000 lines, drifts from the decompiled bodies and loses the address-comment evidence.
- A dual build that uses real MFC on Windows and a shim elsewhere. Rejected: two code paths to keep equal, and the Windows path would hide shim gaps.
- SDL2 from the distribution. Rejected: SDL3 is the maintained line and the project does not want to migrate later.
- A 32-bit-only build. Rejected: modern Linux toolchains and users are 64-bit; 32-bit stays a Windows preset for the oracle.

## Consequences

- Decompiled bodies stay close to the binary, so diffs against upstream and the original remain readable.
- The shims must be tested; `compat/` and `platform/` build with warnings as errors and have unit tests.
- Dependencies are downloaded at configure time, so the first configure needs network access.
- Some casts in kept code needed pointer-width fixes; on-disk formats are unaffected because packed structs are untouched.
- Win32 semantics (message queue, polled input) remain visible in the platform layer, which a later render API can build on.
