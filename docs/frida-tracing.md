# Differential Frida tracing

The toolchain answers one question: over the same scenario, does the
original's engine and ours do the same thing at a chosen function — same
calls, same order, same arguments, same returns? It hooks the function in
both binaries with Frida, records one JSONL trace per side, normalizes the
noisy parts away (process-local pointers, timestamps, harness events) and
diffs what is left.

Adopted from the WSO fork (docs/THIRD_PARTY.md); the hook-table schema, the
verdict discipline and the comparison machinery are his, rehosted on our
sandbox pipeline. What is dropped on purpose is listed at the bottom.

## Files

| File | Role |
|---|---|
| `tools/frida/hooks.py` | hook table (JSON) -> Frida JS; the schema docstring is the reference |
| `tools/frida/orig.py` | the sidecar: spawn (`--exe`) or attach (`--attach`), run a table, write the trace and a verdict |
| `tools/frida/probe.py` | one side per Windows Sandbox: stages the tools and the scenario, launches the game the way a replay record does, brings the trace home |
| `tools/frida/trace_diff.py` | normalize and compare two traces; `compare` is the smoke gate |

## The Frida venv

The sidecar runs in a venv the host already vetted, mapped read-only into
every sandbox at its host path (probe.py maps the venv and its base
interpreter, `C:\Python314`, into the box):

```
C:\Python314\python.exe -m venv %USERPROFILE%\iwd2r-frida\venv
%USERPROFILE%\iwd2r-frida\venv\Scripts\python.exe -m pip install frida==17.24.1
```

Measured: frida 17.24.1 on Python 3.14.8 (MSC v.1944, 64 bit). The games are
32-bit; the 64-bit runtime attaches to them fine (spawn and attach both
verified against IWD2.exe and our iwd2r.exe) — the interpreter does not need
to match the target's bitness.

## Hook tables

The schema is documented in `tools/frida/hooks.py`'s docstring; the short
version: a table is `{"process", "log", "hooks": [...]}` and each hook names
a function entry (`conv` thiscall/cdecl/stdcall, optional stack `args`,
`this_dump`, `globals` chains, `bt` backtrace, `ret` return logging, `max`
hits). Hook function entries only — Frida's x86 trampoline rewrites ~5 bytes
and corrupts anything mid-function.

Addresses differ by side:

- **The original** takes absolute addresses (`"addr": "0x73F560"`): ImageBase
  0x400000, no ASLR, fixed for the vanilla `IWD2.exe`.
- **Ours** takes RVAs (`"rva": "0x1A7CA0"`): our addresses move with every
  rebuild, so the table carries an offset from the module base and orig.py
  resolves it in the target at install time.

Ours-side RVAs are read from the build's `RelWithDebInfo` PDB public symbols
(S_PUB32 records): each record carries a name, a 1-based PE section and an
offset; RVA = section table `VirtualAddress` + offset. The records live in
the DBI's symbol-record stream (MSF stream 3, the u16 at DBI+0x14 names it;
DBI+0x10 is the hash stream, which holds no records — reading that one gives
zero symbols and looks like a bug in your parser). No PDB tool is installed
on this machine (no llvm-pdbutil, no DIA build), so the walk is ~30 lines of
Python against the record layout: `reclen u16, kind u16 (0x110E for
S_PUB32), flags u32, off u32, sect u16, name z-string`. Re-resolve after
every rebuild — the numbers are per-build, not project constants (e.g.
`?MoveToPoint@CGameSprite@@QAEFXZ` was RVA 0x12B2D0 in the 2026-10-12 build
and 0x1A7CA0 in the 2026-10-18 one; the same query on the old build's PDB
reproducing the old number is how the parser was validated).

## Running one side

    python3 -I tools/frida/probe.py run --target original \
        --probe <table-for-original.json> --scenario world_entry \
        --seed 1 --clicks 3 --hit MTP

    python3 -I tools/frida/probe.py run --target ours \
        --probe <table-for-ours.json> --scenario world_entry \
        --seed 1 --clicks 3 --hit MTP --loaded-rva 0x330000

`plan` stages the same run and prints the sandbox plan (folders, logon
command) without starting a box — use it to check a new table's staging
before spending a launch.

- **original**: the oracle build's injector + recorder.dll drive the vanilla
  `IWD2.exe` exactly as `replay record` does; the Frida sidecar
  (`orig.py --attach IWD2.exe`) runs beside it and watches.
- **ours**: `orig.py --exe` spawns our `RelWithDebInfo` build under Frida;
  RunScenario drives the same scenario in-engine. `--loaded-rva` points the
  `loaded` observation at `CScreenWorld::EngineActivated` (the default,
  0x6869C0, is the original's absolute address; ours needs the RVA).
- `--clicks N` inserts N `click 400 300` actions (each followed by a wait)
  before the scenario's trailing `quit`, on a run-folder-local copy — the
  repo's scenarios/ tree is never written. A click is what drives
  `CGameSprite::MoveToPoint`; a look-only scenario would fire nothing and
  end NOT-EXERCISED.
- `--hit NAME --hit-min 1` makes CLEAN require the hook to have fired at
  least once: a green run cannot be a false green.
- Locks nest as in a replay record (REPLAY_LOCK outside SANDBOX_LOCK); one
  sandbox at a time. The trace and verdict come home under
  `runs/<stamp>/replay/`; captures stay under runs/ and are never committed.

## Comparing

    python3 -I tools/frida/trace_diff.py compare <orig.jsonl> <ours.jsonl>

Exit codes: 0 match, 1 behavioural difference (frequency/order/argument
differences are reported separately), 2 malformed or empty input. By default
the comparison drops harness tags (`HITS`, `drive`, `loaded`, `ready`,
`sites`), ignores per-run fields (`bt`, timestamps, pids, threads) and
canonicalizes pointer-valued fields to presence (`<ptr>`/`<null>`) — the two
processes never agree on addresses.

## Verdicts

orig.py writes a status file whose last line is `RESULT: <verdict>`; probe.py
maps it to its exit code: CLEAN 0, CRASH 1, NOT-LOADED / NOT-EXERCISED /
OBSERVER-ERROR all 2. The 2s mean *ran, proved nothing* — same discipline as
a smoke's `--hit`, so a soft failure cannot masquerade as a pass.

Dropped from the fork on purpose: his in-target load driver (cursor
calibration, movie skip, the click driver — the house pipeline drives
scenarios with recorder.dll / RunScenario on both sides), and his
PROCESS-EXITED verdict (a scenario ends with `quit`, so the process exiting
after the world screen came up is what success looks like; dying before
`loaded` is still NOT-LOADED).

## Measured smoke (PENDING)

The smoke gate — `CGameSprite::MoveToPoint` (original absolute 0x73F560 /
ours RVA 0x1A7CA0, `ret: "u16"`) over the `world_entry` replay, original
side then ours side, then `trace_diff.py compare` — did **not** complete.
Ruling 16 time-boxed the sandbox round-trips after the fourth attempt; the
slot is owed to the first wave task that needs a probe. What the four
original-side launches measured (ours side never launched):

1. Two runs (2026-10-18 04:03, and one earlier): the box refused the whole
   configuration — probe.plan_folders mapped
   `<HostFolder>/mnt/c/Python314</HostFolder>` (a WSL path) and Windows
   Sandbox rejects any non-Windows HostFolder outright: nothing executes,
   `wait_done` times out after 540 s, exit 2. Fixed in plan_folders (every
   host path now goes through `wslpath_w`); pinned by
   `ProbePlanTest.test_mapped_host_paths_are_windows_paths`.
2. 2026-10-18 04:16: the box booted and the replay driver ran the whole
   scenario (`status.txt`: frame=2623, state=finished; world_settled
   checkpoint; world.bmp) — but the sidecar died at its first attach poll:
   `SETUP-ERROR InvalidArgumentError: process not found` /
   `RESULT: OBSERVER-ERROR (MTP hit x0)`, exit 2. Root cause: frida
   17.24.1's `get_process_by_name` raises `InvalidArgumentError`, not
   `ProcessNotFoundError`, when the process is not there yet; the poll loop
   caught only the latter. Fixed in orig.py's attach_when_there (TDD:
   `OrigAttachTest`, stub frida module, RED reproduced the box failure
   verbatim, then GREEN).
3. 2026-10-18 04:25: `=== attaching IWD2.exe (1 hooks) ===` / `ARMED
   pid=8252` / `loaded: world screen up` — hooking and both addresses
   proven live — then `probe: frida-trace.jsonl.status has no RESULT line`,
   exit 2. The trace carries `sites`/`ready`/`loaded` but no MTP hit yet:
   the driver's done.txt (scenario end, ~3 s after `loaded` under the
   virtual clock) released `wait_done` while the sidecar was still inside
   its 20 s post-load window; the probe read the verdict file, found no
   RESULT, and closed the box, killing the sidecar mid-window. Diagnosis:
   done.txt measures the *driver*; on the probe path the host should wait
   for the sidecar's `RESULT` line instead (bounded — orig.py hard-caps at
   `--timeout timeout+90`). The fix is **not** landed: it needs its own
   smoke to validate, which Ruling 16 defers.

Until the smoke lands, the comparison machinery is verified offline (same
session, measured):

    $ python3 -I tools/frida/trace_diff.py compare fx-orig.jsonl fx-ours.jsonl
    TRACE-DIFF MATCH events=2/2
      frequency: match
      order: match
      arguments: match
    RESULT: MATCH          # exit 0 — fixtures differing only in
                           # process-local pointers and timestamps
    $ python3 -I tools/frida/trace_diff.py compare fx-orig.jsonl fx-ours.jsonl
    TRACE-DIFF DIFFERENT events=2/2
      frequency: match
      order: match
      arguments: 1 difference(s)
        MTP[2].ret: 65535|0
    RESULT: DIFFERENT      # exit 1 — one ret field changed

When the slot comes: relaunch the original side with the RESULT-wait fix,
expect `RESULT: CLEAN` with MTP hits, then the ours side
(`--loaded-rva 0x330000`, probe rva 0x1A7CA0), then
`trace_diff.py compare` — exit 0 or the measured difference, reported as
measured.
