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
every rebuild (`truth.parse_map` on the build's `iwd2r.map` does the same in
one call; `tools/play.py build_windows_x86()` builds the x86 RelWithDebInfo
exe and map: `python3 -I -c "import sys; sys.path.insert(0,'tools'); import
play; play.sync_windows(); play.build_windows_x86()"`; `tools/gates.sh
--windows` runs it too) — the numbers are per-build, not project constants (e.g.
`?MoveToPoint@CGameSprite@@QAEFXZ` was RVA 0x12B2D0 in the 2026-10-12 build
and 0x1A7CA0 in the 2026-10-18 one, 0x1A8A90 in the 2026-10-23 one
(`?EngineActivated@CScreenWorld@@UAEXXZ` was 0x330E60 there); the same query on the old build's PDB
reproducing the old number is how the parser was validated).

## Running one side

    python3 -I tools/frida/probe.py run --target original \
        --probe <table-for-original.json> --scenario world_entry \
        --seed 1 --clicks 3 --hit MTP

    python3 -I tools/frida/probe.py run --target ours \
        --probe <table-for-ours.json> --scenario world_entry \
        --seed 1 --clicks 3 --hit MTP --hit-min 1 --loaded-rva 0x330E60

The original-side table hooks `CGameSprite::MoveToPoint()` (the no-argument
action handler, `QAEFXZ`) at 0x73F560; 0x73FEC0 is the `(dest, range)`
overload, a different function. `--post-load SECONDS` is how long the sidecar
keeps tracing once `loaded` is seen (default: the scenario's `# timeout: S`
header, else 180; the sidecar's own default of 20 s ended ours before the
trailing clicks).

The sandbox run waits for the verdict. The logon command is `cmd.exe /c
<sidecar> & <driver> & <wait> & echo done > done.txt & shutdown /s /t 0`
(ours has no driver): `<wait>` is a powershell one-liner that polls the
sidecar's status file for a `RESULT:` line, bounded by scenario timeout +
60 s. The original side's replay driver runs with `shutdown` off for a probe
(it used to power the box off when the game exited, killing the background
sidecar before its verdict), and the host keeps the box open until the
status file carries `RESULT:` because that driver writes its own `done.txt`
when the game exits.

On the original side the host has its own deadline (scenario timeout + 60 s
for the `RESULT:` line, after the replay driver's `done.txt`) and the driver's
shutdown is off.

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
- `--build-checkout CHECKOUT` runs a side from another Windows checkout:
  ours its x86 build, the original its oracle build's injector and
  recorder. The driver's Frida phase builds each batch worktree in its own
  checkout (`%USERPROFILE%\iwd2r-build-<worktree name>`: `iwd2r`,
  `iwd2r_injector`, `iwd2r_recorder`) and passes it for both sides, with the
  hook table's RVAs from that build's map, so the bodies under test are the
  batch's and the recorder writing the original's dumps comes from the same
  tree as the dumper writing ours (one schema.json; a recorder.dll older
  than the schema in the driver's checkout failed every b014/i001 dump
  guard). The default is this checkout's builds; the sidecar tools are
  staged from this checkout either way.
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

A script error also writes `ERROR script: <description> (<file>:<line>)` to
the status file (it used to reach only the trace). A hook site Frida refuses
("unable to intercept function at ...", e.g. a stub that is `ret 4` and int3
padding, shorter than Frida's 5-byte jump) is caught per hook: the status
file gets `UNHOOKED <name> at <addr>: <error>` and no HITS line for it, the
other hooks, the `loaded` observation and the exit-time totals still
install, and the run is not an OBSERVER-ERROR. That row reads `not
measured` ("Frida could not hook it on ours in <scenario>"). Before this,
one refused site aborted the whole script: i001's ours runs ended
OBSERVER-ERROR with `HITS-FINAL no` on CInfGame::CacheResFileWithResource's
3-byte stub.

Dropped from the fork on purpose: his in-target load driver (cursor
calibration, movie skip, the click driver — the house pipeline drives
scenarios with recorder.dll / RunScenario on both sides), and his
PROCESS-EXITED verdict (a scenario ends with `quit`, so the process exiting
after the world screen came up is what success looks like; dying before
`loaded` is still NOT-LOADED).

## Measured state

As measured on 2026-10-23, newest first:

- The validation pair (final fix wave): a disposable import batch in a
  scratch worktree (`p3/frida-validate`), row 0x46B160
  `CGameArea::GetNearest`, `world_entry` only, through the driver's own
  phase (the scratch worktree built for x86 in its own checkout; ours hooked
  at that build's RVA: 0x10FE10 for the stubs, 0x7F070 for the full body).
  A stub returning `INVALID_INDEX` read **equal**: the original returns -1
  on all 12 of `world_entry`'s calls, so that scenario cannot tell such a
  stub from the body (an equal row is equal on the inputs the scenarios
  exercise, no more). A stub returning 0 read **different** (event 2: ret
  -1 against 0) and drew the park finding; the full body read **equal**
  (12/12 calls, final totals, 27 checkpoints and 87 periodic dumps
  matching) and the driver rewrote its evidence to `kind: trace` with a
  `compare=0` line that `check_evidence` and the close accept. Runs
  `runs/20261023-144231` (original, reused for the two later ours runs),
  `-144325`, `-144634`, `-144839`; four launches.
- The calibration (`truth.py calibrate`, the 28 calibration rows, all three
  scenarios, both sides under the replay clock, typed tables, exit-time hit
  totals; runs `runs/20261023-1321..1328`): every side `RESULT: CLEAN` with
  `HITS-FINAL yes`, frame counts equal (world_entry 2623, first_fight 5924,
  walk_and_talk 4839), every checkpoint and periodic dump equal (27/87,
  39/197, 36/158). Frida measured 13 of 28 rows; re-derived under the
  frequency rule (no launches) it agrees on 12 and disagrees on 1
  (0x704D40, an argument at event 13 of first_fight); 0x7941D0's call
  counts differ by 23 to 31 per scenario with every captured event equal, a
  caller finding. `docs/wso/truth-calibration.json` holds the record.
- The MoveToPoint pair on `walk_and_talk` (before the replay clock): both
  sides CLEAN (283 and 310 hits), compare DIFFERENT on two returns; the
  pipeline worked end to end, the difference was the free-running clock.

The rest of this section is the history of the smoke, kept for its
diagnoses.

2026-10-23 (after the sidecar wait, `--post-load` and the x86 build): both
sides now finish with a verdict, `RESULT: NOT-EXERCISED (MTP hit x0)` each,
over `world_entry --clicks 3`. At the end of that scenario the world has a
dialog open (`dialog.active 1`) and the party member's position does not
change across the three clicks (original dumps, frames 2430 to 2610), so
`click 400 300` never reaches `MoveToPoint`: the smoke needs a scenario that
closes the dialog and selects the character before its clicks.

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

State as measured 2026-10-23: the RESULT-wait fix has landed and both sides
now end with a verdict, but the `world_entry` pair ends NOT-EXERCISED on both
sides because the scenario ends in a dialog (the clicks never move a sprite).
RVAs in the 2026-10-23 map: `CScreenWorld::EngineActivated` 0x330E60
(`--loaded-rva`), `CGameSprite::MoveToPoint` 0x1A8A90; the original's no-arg
`MoveToPoint` is 0x73F560 (0x73FEC0 is the `(dest, range)` overload). Next:
a scenario that walks the party (`walk_and_talk`), then `trace_diff.py
compare`, exit 0 or the measured difference, reported as measured.

## Typed tables and the replay clock (truth.py batches)

`truth.hook_table` types a row from our prototype when our map has the
function and `src/*.h` declares it: references and pointers are `ptr`
(trace_diff canonicalises them), `BYTE`/`BOOLEAN` `u8`, `SHORT` `s16`,
`WORD` `u16`, `BOOL`/`LONG`/`int` `s32`, `DWORD` `u32`; `void` logs no return.
Ghidra's guesses (`int` for pointers, a `uint` return for a void function)
read stack and EAX leftovers as values and made equal functions look
different. Overloads in a header are picked by the evidence signature's
argument count and typed only when they agree; a class or enum by value, a
float, or no declaration falls back to the evidence signature, and the
table's `typed_from` says `ours` or `ghidra` per row.

`probe.py`'s ours side sets `IWD2R_STATE_DUMP` (the run's replay folder) and
`IWD2R_SEED`, so platform/replay.cpp runs ours under the same virtual clock
and frame-locked AI the recorder gives the original. Without it ours ran on
the real clock with a free-running AI thread and its call counts and order
could not match.

## Verdicts in a batch (truth.frida_verdicts)

The driver's Frida phase (and `truth.py calibrate`) compares each hooked row
per scenario, then across the three scenarios:

- **Final totals.** The hooks cap what they log (`max: 200`), so equal
  events alone say nothing about how often a function ran. The agent script
  sends one last `HITS` message, marked final, when the process calls
  `ExitProcess` or `TerminateProcess` on itself, and waits for the sidecar's
  ack; the status file then says `HITS-FINAL yes`. The 2 s timer's totals
  lose a run-dependent tail, so a scenario whose events compare equal but
  lacks final totals on either side is `not measured`.
- **Frequency is a caller finding.** When every captured event compares
  equal but the final totals differ, the row is still `equal` in that
  scenario; so is the prefix case, one side's events a strict prefix of the
  other's with that side under the cap, when both sides have final totals
  (without them it is `not measured`: a killed run's shorter trace proves
  nothing). The difference goes out of band (`info['notes']`), and the
  driver writes it to the ledger (`- frida frequency only (a caller
  finding): ...`), for wave-1 and import batches alike, never as a finding
  or a verdict change. One exception: an import row that is equal only as a
  prefix IS parked, since no scenario compared it exactly and a `compare=0`
  citation would be false; wave-1 and the calibration treat it as equal. A
  row called on one side only compares no call: `not measured`, with a note.
- **Different** means an event-level difference: an argument or a return at
  a call both sides made, or a crash event.
- **The dump guard.** Both sides write checkpoint and periodic state dumps
  into their replay folders; `truth.frida_run` compares them per scenario
  with the replay tooling's own semantics (`truth.compare_dumps`): first
  `replay record`'s schema check (`oracle.check_dumps` against
  `oracle/replay/schema.json`; a side the schema refuses gives no verdict,
  and the driver makes that the batch's exception at stage frida, nothing
  parked), then `oracle.compare_checkpoints` and `compare_frames`
  (`screens.json`, the known differences, `TIMING_FIELDS` and
  `OURS_ONLY_FIELDS` skipped). A scenario whose dumps differ measures nothing (its
  rows read `not measured`, "dumps differ in <scenario>"), and the driver
  writes trace evidence only from a run whose dumps match. A void hook logs
  no return, so its trace mostly measures its callers; the dumps speak for
  its body.
- **Citations.** `info['exact']` lists the scenarios whose compare returned
  0; an import row's `trace_source` cites the first of them (whichever
  scenario it is), never a prefix-only one. A row equal only as a prefix
  has nothing to cite and is parked.

Precedence across scenarios: any difference, then a failed scenario, then
not measured, then equal; a row no scenario exercised is `not-exercised`.

The phase measures the batch's own build, so for a wave-1 batch an `equal`
row confirms whichever side the verdict records (ours, neither, or a fork
body the worker adopted: `measured: frida`) and `different` is a finding
for whichever side it is. A phase-wide failure (the x86 build fails, the run
raises, or every scenario failed with no status on either side) is the
batch's exception at stage `frida`: nothing is measured, parked or
rewritten; one row's or one scenario's failure stays a row-level outcome.
