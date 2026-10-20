# Wave 0b: a driver that runs the batches, triage before any agent reads, and measured verdicts

Date: 2026-10-20. Status: draft for Thierry's review. Implements before batch b011 of
the WSO integration plan (`docs/superpowers/plans/2026-10-17-wso-integration.md`);
extends `tools/wave.py` and the brief template from Task 5e of that plan.

## Why

Wave 1 settled 200 of 406 disagreements on 2026-10-18 in about 14 hours and 35,000
Z.ai credits. Measured from the session transcripts and the published scorecard:

- The coordinator model spent a third to a half of every 5-hour window on bounded
  waits, ledger appends, reconciling children and re-reading reports. None of that is
  judgment.
- Review plus fix round plus re-review cost about as much as the implementer, and
  changed a verdict in none of the ten batches; it corrected prose in evidence files.
- Of the 200 verdicts, 179 rest on an agent reading the decompile dump, 21 on a
  replay checkpoint, 0 on an oracle probe. The brief calls a dump reading "a weaker
  verdict"; it was the norm. The ground-truth tools (oracle cases, replay compares,
  the adopted Frida differential tracer) went unused because each probe is hand
  work.
- Rows that a human would decide in seconds (the fork renamed a local, the fork's
  body is a stub against our full one, only the signature line differs) went through
  the same read-adjudicate-review cycle as real disagreements.
- Batches sat up to three hours in their read phase at 65K context because workers
  read whole source files although `fn_digest`, `src_find` and `assemble_context`
  exist since wave 0.

The weekly cap buys about 30 agent-hours. At this rate the recovery waves cost
three weeks of credits. The changes below aim at half that with more verdicts
measured instead of read.

## What changes, in one paragraph

A program runs the batch loop instead of a model: it picks the batch, triages the
rows so only the ambiguous ones reach an agent, dispatches one headless Flash
worker per batch with a digest bundle instead of raw files, runs the mechanical
checks and the gate, dispatches a reviewer only for the rows that can change
engine code or for a seeded sample, writes the ledger entry from a template,
closes and pushes, and moves on. Before a verdict is final the same program tries
to measure it: an auto-drafted oracle case for pure functions, and one batched
Frida differential run per batch for the rest. The AoE coordinator session stays,
but as the exception handler the driver wakes when something it cannot decide
happens.

Three components, each shippable alone, in the order triage, driver, truth.

## Component 1: triage (`tools/triage.py`)

**Input:** `docs/wso/disagreements.json` rows of category `both_changed_differ`,
the fork at the pinned `fork_base` (wave-state), ours at `first`.

**Output:** `docs/wso/triage.json`: one entry per row, `{address, class, reason}`,
and a `class` field copied into each wave-state row when `wave.py next` assigns it.

**Classes**, decided from the two function bodies after the same normalisation
`tools/xrec.py` already applies (comments blanked, whitespace collapsed), plus an
identifier-renaming step (every identifier that is not a keyword, a type name from
`src/*.h`, a call target or a numeric literal is replaced by a positional token):

| class | rule | auto verdict | case id |
|---|---|---|---|
| `rename-only` | bodies identical after renaming | ours | `triage-rename` |
| `signature-only` | bodies identical, only the signature line differs | ours | `triage-sig` |
| `fork-stub` | their body is stubby (`xrec.stubby`) or shorter than a quarter of ours and introduces no call target ours lacks | ours | `triage-stub` |
| `ours-stub` | our body is stubby and theirs is not | none, row leaves wave 1 and joins the import list (`theirs_real_ours_stub`) | |
| `ambiguous` | everything else | none, goes to the agent | |

An auto verdict is recorded through the existing `wave.py verdict` path with the
case id above and `losing_kind` as today. Its evidence line is written
mechanically in the row's evidence file, same phrase the close check greps:
`disagreed with WSO at <fork_base>: <class>; case triage-<class> decides`. The
evidence `kind` is not changed.

**Calibration gate, the part that makes this safe.** Triage runs first over the 200
rows already closed in b001 to b010. For each auto class the agreement with the
recorded verdict must be 100 percent, or the class is disabled (left `ambiguous`)
until its rule is fixed. The calibration numbers go into `docs/wso/xrec-report.md`
next to the scorecard, and `tests/test_tools.py` carries the calibration as a test
over the committed wave-state, so a later rule change that would have flipped a
closed verdict fails the suite.

**Expected effect:** from the reports of the ten closed batches, roughly half the
rows are one of the three auto classes. The re-comparison against Will's tip at
every wave boundary goes through the same triage, so his daily pushes cost agent
time only for real disagreements.

## Component 2: the driver (`tools/wave.py run`)

**Shape.** A long-running process started from a shell (`python3 -I tools/wave.py
run --workers 2 --budget 9000`), not from inside an AoE session. It owns the batch
loop below and nothing else. State is the existing `docs/wso/wave-state.json`
plus an untracked `.superpowers/sdd/<plan>/driver/` folder (status file, worker
logs, review packages, headless transcripts).

**Loop**, per batch, with the existing commands kept as the building blocks:

1. `next` picks the rows and cuts the worktree as today, then applies triage: auto
   rows get their verdicts recorded, `ours-stub` rows are moved to the import list,
   ambiguous rows become the batch.
2. The brief is rendered from the template with a new `{{bundle}}` placeholder: the
   path of an `assemble_context` bundle built for the batch's ambiguous rows (ours
   and fork digests, callers, the evidence drafts). The brief forbids reading a
   whole `src/*.cpp` when the bundle covers the function.
3. One worker runs headless: the `claude-glm` wrapper in print mode with the Flash
   model, `--dangerously-skip-permissions`, `--output-format json`, a turn cap, the
   batch worktree as cwd, the brief as the prompt. Its JSON result and transcript
   land in the driver folder. The worker's report format is unchanged.
4. Mechanical checks before any review: every ambiguous row has a verdict line in
   the report, every touched evidence file carries the exact phrase, the tool
   tests pass, `check_evidence.py` passes, the parity step passes. A failure here is
   one fix round with the same worker, with the check output as the prompt. A
   second failure stops the batch (see exceptions).
5. Review only when it can matter: a headless reviewer runs if the batch has any
   `fork` or `neither` verdict, or if a seeded draw (seed recorded in the ledger)
   picks the batch for the twenty-percent sample of `ours` rows. The review package
   is the diff the coordinator builds today. A BLOCKING finding is one fix round;
   a second BLOCKING stops the batch.
6. `close` as today (gate under the memory cap, fast-forward of `first`), then
   push both refs, then the ledger entry is appended to `progress.md` from a fixed
   template: batch id, rows by class, verdict tally, deciding-evidence tally,
   review outcome, commits, credits estimated for the batch.
7. Gates are serialised by a lock; workers run in parallel up to `--workers`.

**Budget and parking.** The driver estimates credits from the headless
transcripts with the formula `~/.config/zai/glm-burn.py` uses and stops cleanly at
`--budget`. A rate-limit error from the API (429) parks the driver: WIP committed
on the batch branch, status file written, exit code 3. The Friday reset timer starts
`wave.py run` again instead of sending `continue` to a model.

**Exceptions**, the only cases that reach the coordinator session: a second
mechanical-check failure, a second BLOCKING review, a worker that exits without a
report, a gate refusal the driver cannot read, a triage calibration failure. The
driver writes the exception to the status file, appends a ledger line, sends the
AoE session one line with `aoe send`, and continues with the next batch if the
failure is batch-local. The coordinator model handles the exception with the
same tools it uses today and tells the driver to retry the batch by clearing the
status entry.

**Dry run.** `wave.py run --dry-run` executes the loop with a fake worker that
returns a canned report, so the loop, the checks, the ledger template and the
exception paths are unit-tested without credits.

## Component 3: measured verdicts (`tools/truth.py`, called by the driver)

Two measurement paths, tried before a batch is reviewed. Both reuse tools that
already exist; the new part is generating their inputs from the batch rows.

**Pure functions get an oracle case.** From the evidence decompile, a function is
`pure` when it reads only its arguments and constants, writes nothing but its
return value and locals, and calls only functions already marked pure or known
pure helpers (the rules-math set). For each pure ambiguous row `truth.py` drafts
an oracle case source in `oracle/cases/` following the existing case pattern
(the file comments in `oracle/cases/*.cpp` are the template): a sweep over the
argument domains the signature implies, our function against the original through
`CallOriginal`. The driver builds and runs it with `oracle.py run --file`; a clean
run records the verdict with `case probe-<batch>-<addr>` and leaves the file in
place as the function's permanent oracle case. A mismatch is reported to the
worker as a finding, not auto-decided.

**Everything else gets one batched Frida differential run.** From the batch's
rows `truth.py` writes a hook table (the schema `tools/frida/hooks.py` reads:
address in the original, the matching RVA in our build from the build map, the
argument readers the digest implies) and the driver runs `tools/frida/probe.py`
for the original and for ours over the three scenarios once per batch, then
`trace_diff.py compare`. Per function the result is one of: exercised and equal
(verdict recorded with `case frida-<batch>-<scenario>`, `kind: trace` with a
`trace_source` citing both binaries' hashes, as the evidence README requires),
exercised and different (a finding for the worker, with the diff), or not
exercised (stays a dump reading, and the scorecard says so).

**Calibration.** Before the driver trusts either path, `truth.py` runs over the 18
rows the standing ruling already requires probes for (10 fork, 8 neither) and
over a seeded sample of ten dump-decided `ours` rows. The agreement with the
recorded verdicts and the share of rows the paths could measure at all go into
the scorecard. If the Frida path measures fewer than a fifth of the sample, the
driver keeps it off by default (`--no-frida`) and the ruling's probes stay manual.

## Interfaces

- `docs/wso/wave-state.json`: each row gains `class` (triage); each verdict gains
  `measured` (`probe`, `frida`, `replay` or `none`). `wave.py scorecard` already
  splits by deciding kind; it adds the triage classes and the measured column.
- `docs/wso/brief-template.md`: adds `{{bundle}}` and the rule "do not read a
  whole source file the bundle covers".
- `docs/wso/triage.json`: new, committed, regenerated by `wave.py next` and at
  every wave-boundary re-comparison.
- `oracle/cases/wave-*.cpp`: generated cases, committed when they pass.
- `.superpowers/sdd/<plan>/progress.md`: driver entries use a fixed template
  headed `## driver <batch>`; human entries are unchanged.
- The AoE coordinator session is unchanged except that it no longer runs the
  loop; it receives exception lines and keeps the rulings.

## Not in scope

- Waves 2 to 5 (imports) keep `close --kind import`; the driver runs them with the
  same loop once wave 1 is closed, no new design needed.
- The renderer port, project 3, and anything that changes engine code outside a
  verdict.
- Replacing Flash or the full model for any role; the driver only moves the
  mechanical turns off the model entirely.

## Testing

All in `tests/test_tools.py`, standard library only, no credits:

- triage: fixtures for each class (a renamed local, a signature change, a fork stub,
  an ours stub, a real difference), the normaliser, and the calibration test over
  the committed closed batches (100 percent agreement per enabled class).
- driver: the loop with the fake worker through happy path, one fix round, second
  failure to exception, review-sample draw with a fixed seed, the ledger template,
  budget stop, 429 park (simulated by the fake worker's exit).
- truth: purity detection on three known pure and three known impure evidence
  files; case generation for `IcewindMisc::Roll` matches the existing hand-written
  case in shape; hook-table generation for a batch; `trace_diff` compare over the
  recorded fixture traces the Frida tests already use.

Gates: `tools/gates.sh` unchanged; the new tools are tool tests, not engine code.

## Order and estimate

1. Triage with its calibration: a day of Flash work, and it pays for itself on the
   first batch.
2. Driver with dry-run tests, then one real batch (b011) under human watch.
3. Truth paths, calibrated on the 18 ruling rows first, which the ruling needs
   anyway.

Credits: the loop's mechanical turns are gone, half the rows never reach an agent,
review runs on perhaps a third of batches. The expected cost per batch drops from
about 3,500 credits to roughly 1,200, with more verdicts measured. The remaining
206 wave-1 rows plus the re-comparison rows then fit in one week's cap with room
for the import waves to start.
