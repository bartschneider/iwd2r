# ADR 0003: Oracle runtime

Status: accepted

## Context

Project 3 finishes the roughly 20% of the decompiled engine that is missing and fixes what the other 80% gets wrong. To make that safe, each function must be checked against the original game's own code, not against a reading of the decompilation. The original is `iwd2.exe`: PE32, built with Visual C++ 6, image base `0x400000`, no relocations, MFC 6 and the C runtime linked in statically, and imports only from Windows system DLLs. The installed game is also modded with IWD2EE, so it is not the reference for vanilla behaviour.

## Decision

- **A native 32-bit Windows runner.** `oracle/runner` is an x86 Windows program that maps the vanilla `iwd2.exe` into its own process at `0x400000`, resolves its imports for real, and calls the original's functions on the real CPU next to ours. `tools/oracle.py build|run` drives it from WSL. The same exe, launched suspended with a recorder DLL injected, records replays (`oracle.py replay record`).
- **Vanilla first.** The oracle and the replays use a verified vanilla copy of the game (`oracle.py vanilla`). IWD2EE behaviour is checked against the modded original in project 5.
- **VC6 `rand`.** The engine uses the original's generator on every platform: `state = state * 214013 + 2531011; return (state >> 16) & 0x7FFF`, with per-thread state as VC6's multithreaded runtime keeps it. `compat/msvc_rand` provides it and `compat/mfc.h` maps `rand` and `srand` to it. A unit test pins its first values, and an oracle case compares it with the original's `rand`.
- **Reserve `0x400000` from a suspended child.** The runner starts a copy of itself suspended inside a kill-on-close job object, reserves `0x400000`-`0xA21000` in it, then resumes it.
- **DEP off in the runner.** The runner is linked with `/NXCOMPAT:NO` and calls `SetProcessDEPPolicy(0)` in the child. It exits with an error if DEP stays on.
- **One virtual clock.** The recorder (in the original) and the replay mode (in our engine) share one clock: `platform/replay_clock.h`. Frame N is the same moment in every run.
- **One order inside a frame.** Both sides fire a frame's timers when the frame's SynchronousUpdate returns, so the AI update the timer wakes runs after the main thread's render, never beside it (`platform/replay_clock.h`, "The order inside a frame"). With the timers fired at the start of the frame, which of the two first took the visible area's list lock depended on thread scheduling, and the two engines parted at walk_and_talk's frame 1498.

## Alternatives considered

- **Emulation** (Unicorn, QEMU user mode). Rejected: the original calls real Windows APIs, and an emulator would need every one of them re-implemented. A native process gets them for free and runs at full speed.
- **Wine on Linux.** Rejected: it adds a second Windows implementation whose differences from Windows could show up in the comparison, and it puts the oracle on the wrong side of the platform the original ran on. The user's host is Windows.
- **Run IWD2EE's patched exe first.** Rejected: its changes (IEex hooks) would be mixed into every result. Vanilla is a clean baseline, and the IEex layer is a project of its own.
- **Use the host C library's `rand`.** Rejected: glibc's sequence differs from VC6's, so the same seed gave different dice on Linux. That is a faithfulness bug, not a test detail.
- **Reserve the range from inside the runner.** Rejected, because of what was measured: Windows maps the code-page table `C_1252.NLS` at `0x400000` before any code of the process runs, including TLS callbacks. Only a process that is not yet running can keep the range free.
- **Map the image as `SEC_IMAGE` to keep DEP on.** Not needed. It is untested and stays the fallback if a machine's DEP policy is AlwaysOn.
- **Real time with a tolerance.** Rejected: the original's threads and timers make runs differ. A virtual clock advanced once per frame makes them repeatable.

## Consequences

- **Evidence for DEP.** With DEP on, an original structured-exception handler lives in private memory (the mapped image is not a loaded module) and Windows refuses to run it: the process died with exit code 5 (`0xC0000005`), both for a hand-made handler and for a real C++ frame inside the original. Linking with `/NXCOMPAT:NO` and calling `SetProcessDEPPolicy(0)` made both run, so the original's own exception handling works. An uncaught original C++ throw (`0xE06D7363`) is a separate case, and the runner's filter reports it as a crash with the thrown type.
- DEP is off for the whole runner process. On a machine whose system policy is AlwaysOn the runner refuses to start (exit 9).
- Only Windows runs the oracle. CI cannot, because it needs the vanilla exe, which is never committed. CI runs the data-free parts: the PE loader's parser, the generators, `msvc_rand`, the state dump, the Python tool tests and the evidence check.
- Each case file runs in a fresh process, and `.data` is restored before every case except the C runtime's own bookkeeping ranges (`oracle/loader/vanilla.json`). Objects a case allocates are leaked rather than freed.
- The loader refuses any `iwd2.exe` whose MD5 differs from `vanilla.json`. The WinMain call site, the `rand` addresses and the frame hook are recorded there once, with Ghidra.
- Replays are deterministic on both sides because the clock, the seed, the input and the focus events come from the scenario. The shared clock constants cannot drift apart: both sides include one header.
- The original's threads still prevent continuous comparison. The unit of comparison is the checkpoint, taken after input settles.
- Faults deep inside original code that has its own handlers, a lock left held by a crash, and a stack overflow are contained by the fresh process per file and a timeout, not solved. They are listed in [docs/followups.md](../followups.md).
