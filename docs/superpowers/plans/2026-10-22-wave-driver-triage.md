# Wave 0b: driver, triage and measured verdicts — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the wave-1 batches from a program instead of a model, decide the mechanical disagreement rows without an agent, and measure verdicts with oracle cases and batched Frida runs, so the remaining 206 rows and the import waves fit one weekly GLM cap.

**Architecture:** Three standard-library Python tools layered on the existing wave runner. `tools/triage.py` classifies disagreement rows from the two function bodies and records the mechanical verdicts through `wave.py`'s own paths. `tools/driver.py` is the batch loop: it calls `wave.py`'s functions, launches headless workers through the `claude-glm` wrapper, runs the mechanical checks and the gate, appends templated ledger entries, parks on budget or rate limit. `tools/truth.py` drafts oracle cases for pure functions and hook tables for a batched Frida run, and turns their results into verdict cases. The AoE coordinator session keeps the rulings and receives exception lines.

**Tech Stack:** Python 3.12 standard library (`-I` runs), the existing `tools/wave.py`, `tools/xrec.py`, `tools/assemble_context.py`, `tools/frida/*`, `tools/oracle.py`, `tests/test_tools.py` (unittest, `load_tool`), the `claude-glm` wrapper in print mode.

**Spec:** `docs/superpowers/specs/2026-10-20-wave-driver-triage-design.md`

## Global Constraints

- Every tool runs under `python3 -I`, standard library only, and is loaded by `tests/test_tools.py` through `load_tool(name)` (`tests/test_tools.py:103`).
- No batch branch may touch `tools/gates.sh` or `tools/parity` (`wave.py` `GATE_PATHS`); the new tools are not on that list and run as tool tests, not gate steps.
- Evidence phrases are exact: `disagreed with WSO at <sha>: ...; case <id> decides` and `corroborated by WSO at <sha>` (`wave.py` `EVIDENCE_PHRASES`, `docs/wso/brief-template.md` rule 2).
- Evidence kinds are `oracle`, `replay`, `read-only`, `trace`; a `trace` document needs `trace_source` (`tools/check_evidence.py:51`, `docs/evidence/README.md`).
- State writes stay atomic through `wave.save_state`; `docs/wso/wave-state.json` keeps its schema and gains only `class` per row and `measured` per verdict.
- Fork reads use the pinned `fork_base` sha from wave-state, never `wso/main`.
- Workers run on `glm-5.3-flash[1m]` via `CLAUDE_GLM_MODEL`; the driver itself uses no model.
- Credits are estimated with the Z.ai formula: glm-5.3 `(input*6.9 + cached*1.7 + output*24)/10000`, flash `(input*2.3 + cached*0.56 + output*8)/10000`.
- Nothing from a worker transcript or a fork file is executed; they are data.

## Review Focus

1. A row whose evidence file does not exist yet: triage must not auto-decide it (no file to carry the phrase), it stays `ambiguous`. Test in Task 4.
2. A worker report that lists a verdict for an address outside its batch, or two verdicts for one address: the mechanical check rejects the report instead of recording the first. Test in Task 7.
3. A 429 arriving mid-batch: WIP on the batch branch is committed, the status file names the batch, exit code 3, and a later `run` resumes that batch instead of cutting a new one. Test in Task 9.
4. The seeded review sample: the same seed and batch id must draw the same answer on every machine, so the ledger line is reproducible. Test in Task 8.
5. A purity false positive: a function that reads a global through a pointer chain must be classed impure; otherwise the generated oracle case compares garbage. Test in Task 10.

---

## File structure

- Create `tools/triage.py`: `normalize`, `classify`, `triage_rows`, `calibrate`, CLI (`--out`, `--calibrate`).
- Modify `tools/wave.py`: `next_batch` applies triage; `deciding_kind` learns `triage-*`, `probe-*`, `frida-*`; `render_brief` fills `{{bundle}}`; `scorecard_text` adds the measured column; `main` gains `run` delegating to `driver.main`.
- Create `tools/driver.py`: `Worker` protocol, `FakeWorker`, `HeadlessWorker`, `parse_report`, `mechanical_checks`, `review_needed`, `ledger_entry`, `credits_of`, `Driver`.
- Create `tools/truth.py`: `is_pure`, `draft_case`, `hook_table`, `frida_verdicts`, `calibrate`.
- Modify `docs/wso/brief-template.md`: `{{bundle}}`, the no-raw-reads rule, the verdict line grammar.
- Modify `docs/wso/README.md`: a "Running the batches" section.
- Tests: `tests/test_tools.py` classes `TriageTest`, `WaveTriageTest`, `DriverTest`, `TruthTest`.

---

## Component 1: triage

### Task 1: body normaliser and classifier

**Files:**
- Create: `tools/triage.py`
- Test: `tests/test_tools.py` (new class `TriageTest`)

**Interfaces:**
- Consumes: `xrec.blank_comments(text) -> str`, `xrec.stubby(body) -> bool` (`tools/xrec.py:59,99`).
- Produces: `normalize(body: str, type_names: frozenset[str]) -> str`; `callees(body: str) -> frozenset[str]`; `classify(theirs: str, ours: str, type_names: frozenset[str]) -> tuple[str, str]` returning `(class, reason)` with class in `('rename-only', 'signature-only', 'fork-stub', 'ours-stub', 'ambiguous')`; constants `CLASSES`, `AUTO_OURS = ('rename-only', 'signature-only', 'fork-stub')`, `CASE_IDS = {'rename-only': 'triage-rename', 'signature-only': 'triage-sig', 'fork-stub': 'triage-stub'}`.

- [ ] **Step 1: Write the failing tests**

```python
class TriageTest(unittest.TestCase):
    TYPES = frozenset({'CGameSprite', 'int', 'void', 'BOOL'})

    def test_normalize_renames_identifiers_positionally(self):
        a = 'int f(int count){int total=count*2;return total;}'
        b = 'int f(int n){int sum=n*2;return sum;}'
        self.assertEqual(triage.normalize(a, self.TYPES), triage.normalize(b, self.TYPES))

    def test_normalize_keeps_calls_and_literals(self):
        a = 'void f(){Foo(3);}'
        b = 'void f(){Bar(3);}'
        self.assertNotEqual(triage.normalize(a, self.TYPES), triage.normalize(b, self.TYPES))
        self.assertNotEqual(triage.normalize('void f(){Foo(3);}', self.TYPES),
                            triage.normalize('void f(){Foo(4);}', self.TYPES))

    def test_classify_rename_only(self):
        cls, _ = triage.classify('int f(int a){return a+1;}', 'int f(int b){return b+1;}', self.TYPES)
        self.assertEqual(cls, 'rename-only')

    def test_classify_signature_only(self):
        cls, _ = triage.classify('BOOL f(int a){return a+1;}', 'int f(int a){return a+1;}', self.TYPES)
        self.assertEqual(cls, 'signature-only')

    def test_classify_fork_stub(self):
        ours = 'void f(){' + 'Step();' * 40 + '}'
        cls, _ = triage.classify('void f(){// TODO\n}', ours, self.TYPES)
        self.assertEqual(cls, 'fork-stub')

    def test_classify_short_fork_body_with_new_callee_is_ambiguous(self):
        ours = 'void f(){' + 'Step();' * 40 + '}'
        cls, _ = triage.classify('void f(){Other();}', ours, self.TYPES)
        self.assertEqual(cls, 'ambiguous')

    def test_classify_ours_stub(self):
        cls, _ = triage.classify('void f(){Real();Work();}', 'void f(){// TODO\n}', self.TYPES)
        self.assertEqual(cls, 'ours-stub')

    def test_classify_real_difference_is_ambiguous(self):
        cls, _ = triage.classify('int f(int a){if(a>0)return 1;return 0;}',
                                 'int f(int a){if(a>=0)return 1;return 0;}', self.TYPES)
        self.assertEqual(cls, 'ambiguous')
```

- [ ] **Step 2: Run them to see them fail**

Run: `python3 -I -m unittest tests.test_tools.TriageTest -v`
Expected: FAIL, `load_tool('triage')` raises (no such file).

- [ ] **Step 3: Implement `tools/triage.py`**

`normalize` blanks comments, tokenises with one regex for identifiers, numbers and punctuation, keeps C++ keywords, names in `type_names`, call targets (identifier followed by `(`) and literals, and replaces every other identifier with `v<n>` in first-seen order. `callees` returns the call targets. `classify`: strip the signature (text up to the first `{`) from both; `xrec.stubby` on each body; `ours-stub` when ours is stubby and theirs is not; `fork-stub` when theirs is stubby, or `len(theirs) < len(ours) / 4` and `callees(theirs) <= callees(ours)`; `rename-only` when normalised bodies match and the signatures match after normalisation; `signature-only` when bodies match and signatures differ; else `ambiguous`. The reason string names the rule.

- [ ] **Step 4: Run the tests to see them pass**

Run: `python3 -I -m unittest tests.test_tools.TriageTest -v`
Expected: 8 tests OK.

- [ ] **Step 5: Commit**

```bash
git add tools/triage.py tests/test_tools.py
git commit -m "triage: classify disagreement rows from the two bodies"
```

### Task 2: triage over the disagreement list

**Files:**
- Modify: `tools/triage.py`
- Test: `tests/test_tools.py` (`TriageTest`)

**Interfaces:**
- Consumes: `xrec.show(ref, path) -> str`, `xrec.funcs(text) -> dict[name, (addr, body)]` (`tools/xrec.py:50,80`); rows of `docs/wso/disagreements.json` (`file, name, address, category, their_len, our_len`).
- Produces: `triage_rows(rows, fork_ref, ours_ref, show=xrec.show, type_names=None) -> list[dict]` with entries `{address, file, name, class, reason}` for every `both_changed_differ` row; `load_type_names(show, ours_ref) -> frozenset` from `class X`/`struct X` declarations in `src/*.h`; `main(argv)` with `--fork-ref` (default: wave-state `fork_base`), `--ours-ref first`, `--out docs/wso/triage.json`.

- [ ] **Step 1: Write the failing tests**

```python
    def test_triage_rows_uses_injected_show(self):
        files = {('F', 'src/A.cpp'): '// 0x401000\nint f(int a){return a+1;}\n',
                 ('O', 'src/A.cpp'): '// 0x401000\nint f(int b){return b+1;}\n'}
        rows = [{'file': 'src/A.cpp', 'name': 'f', 'address': '0x401000',
                 'category': 'both_changed_differ', 'their_len': 1, 'our_len': 1},
                {'file': 'src/A.cpp', 'name': 'g', 'address': '0x402000',
                 'category': 'same', 'their_len': 1, 'our_len': 1}]
        out = triage.triage_rows(rows, 'F', 'O', show=lambda ref, p: files[(ref, p)],
                                 type_names=frozenset({'int'}))
        self.assertEqual([(r['address'], r['class']) for r in out], [('0x401000', 'rename-only')])

    def test_triage_rows_missing_function_is_ambiguous(self):
        rows = [{'file': 'src/A.cpp', 'name': 'f', 'address': '0x401000',
                 'category': 'both_changed_differ', 'their_len': 1, 'our_len': 1}]
        out = triage.triage_rows(rows, 'F', 'O', show=lambda ref, p: 'int g(){return 0;}',
                                 type_names=frozenset())
        self.assertEqual(out[0]['class'], 'ambiguous')
        self.assertIn('not found', out[0]['reason'])
```

- [ ] **Step 2: Run them to see them fail**

Run: `python3 -I -m unittest tests.test_tools.TriageTest -v`
Expected: FAIL, `triage_rows` undefined.

- [ ] **Step 3: Implement `triage_rows`, `load_type_names`, `main`**

Group rows by file, call `show` once per (ref, file), `xrec.funcs` on each, look the function up by name then by address, classify. `main` writes `--out` atomically (temp file + `os.replace`, like `wave.save_state`) and prints a count per class.

- [ ] **Step 4: Run the tests to see them pass**

Run: `python3 -I -m unittest tests.test_tools.TriageTest -v`
Expected: 10 tests OK.

- [ ] **Step 5: Run it for real and commit the result**

Run: `python3 -I tools/triage.py --out docs/wso/triage.json`
Expected: counts per class printed; file written.

```bash
git add tools/triage.py tests/test_tools.py docs/wso/triage.json
git commit -m "triage: run over the disagreement list, write docs/wso/triage.json"
```

### Task 3: calibration gate

**Files:**
- Modify: `tools/triage.py`, `docs/wso/xrec-report.md` (generated section)
- Test: `tests/test_tools.py` (`TriageTest`)

**Interfaces:**
- Consumes: wave-state batches with `status == 'closed'` and their `verdicts` (`{addr: {side, case, losing_kind}}`); `triage.json` entries.
- Produces: `calibrate(state, triaged) -> dict[class, dict(agree=int, total=int, mismatches=list[addr])]`; `enabled_classes(calibration) -> frozenset` (every class in `AUTO_OURS` with `total > 0` and `agree == total`); `--calibrate` CLI mode printing the table and exiting 1 on any mismatch; `calibration_text(calibration) -> str` for the report.

- [ ] **Step 1: Write the failing tests**

```python
    def test_calibrate_counts_agreement_per_class(self):
        state = {'batches': {'b001': {'status': 'closed', 'rows': [
            {'address': '0x1'}, {'address': '0x2'}],
            'verdicts': {'0x1': {'side': 'ours'}, '0x2': {'side': 'fork'}}}}}
        triaged = [{'address': '0x1', 'class': 'rename-only'},
                   {'address': '0x2', 'class': 'fork-stub'}]
        cal = triage.calibrate(state, triaged)
        self.assertEqual(cal['rename-only']['agree'], 1)
        self.assertEqual(cal['fork-stub'], {'agree': 0, 'total': 1, 'mismatches': ['0x2']})
        self.assertEqual(triage.enabled_classes(cal), frozenset({'rename-only'}))

    def test_calibration_against_committed_batches(self):
        if subprocess.run(['git', '-C', ROOT, 'rev-parse', '--verify', '--quiet',
                           'e8dd16a7^{commit}'], capture_output=True).returncode:
            self.skipTest('fork base not fetched')
        state = json.load(open(os.path.join(ROOT, 'docs/wso/wave-state.json')))
        triaged = json.load(open(os.path.join(ROOT, 'docs/wso/triage.json')))
        cal = triage.calibrate(state, triaged)
        for cls in triage.AUTO_OURS:
            self.assertEqual(cal[cls]['mismatches'], [], cls)
```

- [ ] **Step 2: Run them to see them fail**

Run: `python3 -I -m unittest tests.test_tools.TriageTest -v`
Expected: FAIL on `calibrate`.

- [ ] **Step 3: Implement `calibrate`, `enabled_classes`, `calibration_text`, `--calibrate`**

A triaged auto class agrees when the recorded side is `ours`. `ours-stub` and `ambiguous` are reported but never enabled. Disabled classes are rewritten to `ambiguous` in `triage.json` with the reason `disabled by calibration`.

- [ ] **Step 4: Run the tests and the real calibration**

Run: `python3 -I -m unittest tests.test_tools.TriageTest -v` then `python3 -I tools/triage.py --calibrate`
Expected: tests OK; the table shows each auto class with `agree == total`. If a class mismatches, fix its rule in Task 1 before continuing; the committed test must pass.

- [ ] **Step 5: Add the calibration table to the report and commit**

Append `calibration_text` output under the scorecard in `docs/wso/xrec-report.md` from `wave.write_scorecard` (Task 5 wires it; here write it once by hand through the CLI).

```bash
git add tools/triage.py tests/test_tools.py docs/wso/triage.json docs/wso/xrec-report.md
git commit -m "triage: calibration gate against the closed batches"
```

### Task 4: `wave.py next` applies triage

**Files:**
- Modify: `tools/wave.py` (`next_batch`, `deciding_kind`, new `apply_triage`, `write_triage_evidence`)
- Test: `tests/test_tools.py` (new class `WaveTriageTest`)

**Interfaces:**
- Consumes: `triage.json`, `triage.enabled_classes`, `wave.record_verdict`, `wave._read_evidence(repo, branch, address)` (`tools/wave.py:213`).
- Produces: `apply_triage(state_path, repo, batch_id, triage_path, fork_base) -> tuple[int, str]`: for each row of the assigned batch with an enabled auto class and an existing evidence file in the batch worktree, appends the line `disagreed with WSO at <fork_base>: <class>; case triage-<class> decides` under a `## WSO delta` heading and records the verdict; rows classed `ours-stub` are removed from the batch and appended to `docs/wso/import-list.json` (`{address, file, name, source: 'triage'}`); every row keeps `class`. `next_batch` calls it after cutting the batch and reports `n auto, n import, n ambiguous`. `deciding_kind('triage-rename') == 'triage'`.

- [ ] **Step 1: Write the failing tests**

```python
class WaveTriageTest(unittest.TestCase):
    def _repo(self):
        # a temp git repo with one evidence file and a batch branch, as the
        # existing wave tests build (reuse their helper if one exists)
        ...

    def test_apply_triage_records_auto_verdict_and_phrase(self):
        repo, state_path = self._repo(evidence={'401000'})
        # batch b001 assigned with rows 0x401000 (rename-only) and 0x402000 (ambiguous)
        code, msg = wave.apply_triage(state_path, repo, 'b001', triage_path, 'e8dd16a7')
        self.assertEqual(code, 0)
        state = wave.load_state(state_path)
        self.assertEqual(state['batches']['b001']['verdicts']['0x401000']['case'], 'triage-rename')
        text = wave._read_evidence(repo, 'p3/wso-b001', '0x401000')[0]
        self.assertIn('disagreed with WSO at e8dd16a7: rename-only; case triage-rename decides', text)
        self.assertNotIn('0x402000', state['batches']['b001']['verdicts'])

    def test_apply_triage_skips_row_without_evidence_file(self):
        repo, state_path = self._repo(evidence=set())
        code, msg = wave.apply_triage(state_path, repo, 'b001', triage_path, 'e8dd16a7')
        self.assertEqual(wave.load_state(state_path)['batches']['b001']['verdicts'], {})
        self.assertIn('no evidence file', msg)

    def test_apply_triage_moves_ours_stub_to_import_list(self):
        ...  # row classed ours-stub leaves batch['rows'], appears in import-list.json

    def test_deciding_kind_triage(self):
        self.assertEqual(wave.deciding_kind('triage-stub'), 'triage')
```

- [ ] **Step 2: Run them to see them fail**

Run: `python3 -I -m unittest tests.test_tools.WaveTriageTest -v`
Expected: FAIL, `apply_triage` undefined.

- [ ] **Step 3: Implement**

`write_triage_evidence(worktree, address, cls, fork_base)` edits the file in the batch worktree and commits it on the batch branch (`git -C <wt> commit -m "triage: <class> verdict for <addr>"`). `apply_triage` loads `triage.json`, computes enabled classes from `triage.calibrate`, loops the rows, calls `record_verdict` with `case=CASE_IDS[cls]`. `deciding_kind` maps prefixes `triage-` to `triage`, `probe-` to `oracle`, `frida-` to `trace`, keeping today's mapping for the rest.

- [ ] **Step 4: Run the tests**

Run: `python3 -I -m unittest tests.test_tools.WaveTriageTest tests.test_tools.TriageTest -v`
Expected: all OK.

- [ ] **Step 5: Commit**

```bash
git add tools/wave.py tests/test_tools.py
git commit -m "wave: next applies triage, auto verdicts carry the exact phrase"
```

### Task 5: bundles in the brief and the measured column

**Files:**
- Modify: `tools/wave.py` (`render_brief`, `scorecard_text`, `write_scorecard`), `docs/wso/brief-template.md`
- Test: `tests/test_tools.py` (`WaveTriageTest`)

**Interfaces:**
- Consumes: `assemble_context.bundle(address) -> str` (`tools/assemble_context.py`); template placeholders `{{id}} {{size}} {{branch}} {{files}} {{rows}} {{fork_base}}`.
- Produces: `render_brief(..., bundle_dir=None)`: when `bundle_dir` is given, writes `<bundle_dir>/<addr>.md` per ambiguous row and fills `{{bundle}}` with the list of paths; without it `{{bundle}}` becomes `(no bundles)`. Template gains rule 3: "Read the bundle for a function before its source; do not read a whole `src/*.cpp` the bundle covers." and the verdict line grammar: one line per row, `verdict <0xADDR> <fork|ours|neither> case <id>`, in a section headed `## Verdicts`. `scorecard_text` adds `measured: probe N, frida N, replay N, none N` per side from `verdict.get('measured', 'none')`; `write_scorecard` appends `triage.calibration_text` when `triage.json` exists.

- [ ] **Step 1: Write the failing tests** (`test_render_brief_fills_bundle_paths`, `test_brief_template_has_verdict_grammar`, `test_scorecard_measured_column`)
- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run, see them pass**
- [ ] **Step 5: Commit** `wave: briefs carry context bundles, scorecard shows measured verdicts`

---

## Component 2: the driver

### Task 6: worker protocol, fake worker, report parser

**Files:**
- Create: `tools/driver.py`
- Test: `tests/test_tools.py` (new class `DriverTest`)

**Interfaces:**
- Produces: `Result = namedtuple('Result', 'exit_code report transcript credits rate_limited')`; class `FakeWorker(report, exit_code=0, rate_limited=False)` with `run(brief: str, cwd: str) -> Result`; class `HeadlessWorker(model='glm-5.3-flash[1m]', max_turns=400, wrapper='claude-glm')` building `argv = [wrapper, '-p', brief, '--output-format', 'json', '--dangerously-skip-permissions', '--max-turns', str(max_turns)]` with `env['CLAUDE_GLM_MODEL']=model`, running it with `cwd`, reading `result` and `session_id` from the JSON, locating the transcript under the projects directory for `cwd`, computing `credits_of(transcript)`; `parse_report(text) -> dict[addr, (side, case)]` from the `## Verdicts` grammar, raising `ReportError` on a duplicate address or a malformed line; `credits_of(transcript_path) -> float` summing per-turn usage with the two Z.ai formulas by model name.

- [ ] **Step 1: Write the failing tests**

```python
class DriverTest(unittest.TestCase):
    REPORT = ('# report\n\n## Verdicts\n\nverdict 0x401000 ours case dump-401000\n'
              'verdict 0x402000 fork case probe-b011-402000\n')

    def test_parse_report(self):
        self.assertEqual(driver.parse_report(self.REPORT),
                         {'0x401000': ('ours', 'dump-401000'), '0x402000': ('fork', 'probe-b011-402000')})

    def test_parse_report_rejects_duplicate(self):
        with self.assertRaises(driver.ReportError):
            driver.parse_report(self.REPORT + 'verdict 0x401000 fork case x\n')

    def test_credits_of_uses_both_formulas(self):
        lines = [json.dumps({'type': 'assistant', 'message': {'model': 'glm-5.3-flash',
                 'usage': {'input_tokens': 10000, 'cache_read_input_tokens': 100000, 'output_tokens': 1000}}})]
        with tempfile.NamedTemporaryFile('w', suffix='.jsonl', delete=False) as f:
            f.write('\n'.join(lines))
        self.assertAlmostEqual(driver.credits_of(f.name), (10000*2.3 + 100000*0.56 + 1000*8)/10000)

    def test_headless_worker_argv(self):
        w = driver.HeadlessWorker()
        argv, env = w.command('BRIEF')
        self.assertEqual(argv[:3], ['claude-glm', '-p', 'BRIEF'])
        self.assertIn('--dangerously-skip-permissions', argv)
        self.assertEqual(env['CLAUDE_GLM_MODEL'], 'glm-5.3-flash[1m]')
```

- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement** (`HeadlessWorker.run` shells out with `subprocess.run(..., capture_output=True, text=True)`; the transcript path is `~/.claude/projects/<cwd with / replaced by ->/<session_id>.jsonl`, the same slug Claude Code uses for this repo's worktrees)
- [ ] **Step 4: Run, see them pass**
- [ ] **Step 5: Commit** `driver: worker protocol, headless launcher, report parser, credit estimate`

### Task 7: mechanical checks and the review policy

**Files:**
- Modify: `tools/driver.py`
- Test: `tests/test_tools.py` (`DriverTest`)

**Interfaces:**
- Consumes: batch dict from wave-state, the batch worktree, `wave._read_evidence`, `wave.EVIDENCE_PHRASES`.
- Produces: `mechanical_checks(batch, worktree, verdicts, run=subprocess.run) -> list[str]`: every ambiguous row has a verdict; no verdict for an address outside the batch; each verdicted row's evidence file in the worktree carries one of the phrases; `python3 -I tests/test_tools.py` passes; `python3 -I tools/check_evidence.py --base first` passes; `python3 -I tools/parity/run.py --base first` passes (skip rows allowed). Returns the list of problems, empty when clean. `review_needed(batch, verdicts, seed, sample=0.2) -> tuple[bool, str]`: True with reason `fork/neither rows` when any verdict side is not `ours`, else True with reason `sample` when `random.Random(f'{seed}:{batch_id}').random() < sample`, else False.

- [ ] **Step 1: Write the failing tests** (`test_mechanical_checks_flags_missing_and_foreign_verdicts`, `test_mechanical_checks_requires_phrase`, `test_review_needed_for_fork_rows`, `test_review_sample_is_deterministic` asserting the same `(bool, reason)` twice for seed `2026-10-22` and batch `b011`)
- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement** (the three subprocess steps are injectable through `run` so the tests pass a stub recording the argv)
- [ ] **Step 4: Run, see them pass**
- [ ] **Step 5: Commit** `driver: mechanical checks before any review, seeded review sampling`

### Task 8: ledger template and the batch loop

**Files:**
- Modify: `tools/driver.py`, `tools/wave.py` (`main` gains `run`)
- Test: `tests/test_tools.py` (`DriverTest`)

**Interfaces:**
- Consumes: `wave.next_batch`, `wave.render_brief`, `wave.record_verdict`, `wave.close_batch`, `wave.write_scorecard`; `aoe send <session-id> <line>` for exceptions.
- Produces: `ledger_entry(batch, verdicts, classes, review, commits, credits) -> str` headed `## driver <batch_id> (<utc timestamp>)` with lines for rows by class, verdict tally, deciding-evidence tally, review outcome, commit range, credits; class `Driver(repo, state_path, worker, reviewer, ledger_path, status_path, aoe_session=None, seed='2026-10-22', budget=None, workers=1)` with `run_batch(batch_id) -> str` returning one of `closed`, `exception`, `parked`, and `run() -> int` looping `next_batch` until `nothing left` (exit 0), budget reached (exit 0 after writing status), or park (exit 3). Fix rounds: at most one per check stage; a second failure writes `status['exceptions'][batch_id]` and sends the AoE line. `wave.py run [--workers N] [--budget C] [--dry-run] [--seed S]` builds the Driver with `HeadlessWorker` or `FakeWorker`.

- [ ] **Step 1: Write the failing tests**

```python
    def test_ledger_entry_shape(self):
        text = driver.ledger_entry(batch={'id': 'b011', 'rows': [...]}, verdicts={...},
                                   classes={'rename-only': 8, 'ambiguous': 12},
                                   review=(False, 'not sampled'), commits='a1..b2', credits=1180.5)
        self.assertTrue(text.startswith('## driver b011 ('))
        self.assertIn('rename-only 8', text); self.assertIn('credits 1180', text)

    def test_run_batch_happy_path_closes(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))   # fake wave functions injected
        self.assertEqual(d.run_batch('b011'), 'closed')
        self.assertIn('## driver b011', open(d.ledger_path).read())

    def test_run_batch_second_check_failure_is_exception(self):
        d = self._driver(worker=driver.FakeWorker('# report\n## Verdicts\n'))  # no verdicts, twice
        self.assertEqual(d.run_batch('b011'), 'exception')
        self.assertIn('b011', json.load(open(d.status_path))['exceptions'])
```

- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement** (`Driver` takes the wave functions as attributes so tests replace them; `--dry-run` uses `FakeWorker` with a report built from the batch's rows, all `ours`, case `dry-<addr>`, and never calls `close_batch`'s gate: pass `gate=False` through a new `close_batch(..., gate=True)` keyword)
- [ ] **Step 4: Run, see them pass; then the real dry run**

Run: `python3 -I tools/wave.py run --dry-run --budget 0`
Expected: one batch cut, triaged, fake-worked, checked, ledger entry written, no gate, exit 0; `git -C <worktree> status` shows the auto-verdict commits only.

- [ ] **Step 5: Commit** `driver: the batch loop with templated ledger entries and exception hand-off`

### Task 9: budget, rate limit, park and resume

**Files:**
- Modify: `tools/driver.py`, `docs/wso/README.md`
- Test: `tests/test_tools.py` (`DriverTest`)

**Interfaces:**
- Produces: `Result.rate_limited` set by `HeadlessWorker` when the JSON result or transcript contains `Request rejected (429)`; `Driver.run` stops with `parked` when a worker returns `rate_limited` or when `credits_spent + estimate_next > budget`; park commits WIP in the batch worktree (`git add -A && git commit -m "WIP: parked by driver"`), writes `status['parked'] = {batch_id, at, reason}`, exits 3; a later `run` reads `status['parked']` and resumes that batch (re-renders the brief with a `Resume:` preamble listing recorded verdicts) before cutting new ones. README section "Running the batches" documents `wave.py run`, the status file, the exit codes, and the Friday timer command `systemd-run --user --unit=zai-resume-driver --on-calendar=... python3 -I tools/wave.py run --workers 2 --budget 50000` (the timer itself is user-side, not in the repo).

- [ ] **Step 1: Write the failing tests** (`test_park_on_rate_limit_commits_wip_and_exit_3`, `test_resume_picks_parked_batch_first`, `test_budget_stop_before_next_batch`)
- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run, see them pass**
- [ ] **Step 5: Commit** `driver: budget stop, 429 park with WIP, resume of the parked batch`

---

## Component 3: measured verdicts

### Task 10: purity detector and oracle case drafts

**Files:**
- Create: `tools/truth.py`
- Test: `tests/test_tools.py` (new class `TruthTest`)

**Interfaces:**
- Consumes: evidence files (`docs/evidence/<ADDR>.md`: front matter, `Decompiled (Ghidra)` section, `Callers`, `Callees`); the existing case pattern `ORACLE_CASE("name", Fn)` with `ctx.CallOriginal([&]{...})` (`oracle/cases/rand.cpp:159`); `tools/check_evidence.py` `RULES_MATH` set.
- Produces: `is_pure(evidence_text, pure_names=frozenset()) -> tuple[bool, str]`: impure on any `DAT_`, `*(…*)(…+ 0x…)` global read through a pointer chain, call to a function not in `pure_names`, write through a pointer parameter, or `this` member write; pure otherwise, reason names the first offending line. `draft_case(address, name, signature, batch_id) -> str`: a C++ case following `oracle/cases/rand.cpp`, one `ORACLE_CASE("wave/<batch>/<addr>", ...)` sweeping each integer argument over `{min, -1, 0, 1, 7, max}` and comparing return values; `write_cases(batch_id, rows) -> path` into `oracle/cases/wave-<batch>.cpp`; `probe_verdicts(report_json) -> dict[addr, bool]` from `oracle.py run`'s report (`cases[].name`, `case_ok`).

- [ ] **Step 1: Write the failing tests**

```python
class TruthTest(unittest.TestCase):
    def test_is_pure_accepts_arithmetic_on_args(self):
        ok, _ = truth.is_pure('## Decompiled (Ghidra)\nint f(int a,int b){return (a*3+b)%7;}\n')
        self.assertTrue(ok)

    def test_is_pure_rejects_global_through_pointer_chain(self):
        ok, why = truth.is_pure('## Decompiled (Ghidra)\nint f(int a){return *(int*)(DAT_008cf6d8+0x1c54)+a;}\n')
        self.assertFalse(ok); self.assertIn('DAT_008cf6d8', why)

    def test_is_pure_rejects_unknown_callee(self):
        ok, _ = truth.is_pure('## Decompiled (Ghidra)\nint f(int a){return Helper(a);}\n')
        self.assertFalse(ok)
        ok, _ = truth.is_pure('## Decompiled (Ghidra)\nint f(int a){return Helper(a);}\n', frozenset({'Helper'}))
        self.assertTrue(ok)

    def test_draft_case_matches_rand_case_shape(self):
        src = truth.draft_case('0x5845D0', 'IcewindMisc::Roll', 'int Roll(int dice, int sides)', 'b011')
        self.assertIn('ORACLE_CASE("wave/b011/5845D0"', src)
        self.assertIn('CallOriginal', src)
        self.assertNotIn('#include <windows.h>', src)
```

- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement** (`is_pure` parses only the `Decompiled (Ghidra)` section; regexes for `DAT_[0-9a-f]+`, `\*\s*\(`, `->`, `this`, calls `\b([A-Za-z_][\w:]*)\s*\(`; the draft is a string template with the sweep table generated per parameter type from the signature)
- [ ] **Step 4: Run, see them pass**
- [ ] **Step 5: Commit** `truth: purity detector and oracle case drafts for pure functions`

### Task 11: hook tables and the batched Frida run

**Files:**
- Modify: `tools/truth.py`
- Test: `tests/test_tools.py` (`TruthTest`)

**Interfaces:**
- Consumes: the hook schema of `tools/frida/hooks.py` (`addr`/`rva`, `conv`, `args`, `ret`, `max`); `tools/frida/probe.py run --target original|ours --probe hooks.json --scenario <name> [--loaded-rva]`; `tools/frida/trace_diff.py` `load_trace`, `compare`; the build map of our exe (the `.map` next to `iwd2r.exe` the Windows build writes) for our RVAs.
- Produces: `hook_table(rows, signatures, our_map, scenarios) -> dict` with one hook per row for both targets (absolute `addr` for the original, `rva` for ours, `conv` from the signature's `__thiscall`/`__cdecl`, `args` from the parameter types, `ret` from the return type, `max` 200); `frida_run(batch_id, table, scenarios, run=subprocess.run) -> dict[scenario, (orig_trace, ours_trace)]`; `frida_verdicts(traces, rows) -> dict[addr, ('equal'|'different'|'not-exercised', detail)]` using `trace_diff`; the driver records `frida-<batch>-<scenario>` for `equal` rows with `kind: trace` and a `trace_source` of the form `frida <probe> orig:<sha256> ours:<sha256> compare=0` (the form `check_evidence.py` accepts), hands `different` rows to the worker as findings, and leaves `not-exercised` rows as they are.

- [ ] **Step 1: Write the failing tests** (`test_hook_table_from_signatures`, `test_frida_verdicts_equal_different_not_exercised` over the fixture traces the existing Frida tests use)
- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement** (`frida_run` is host-side: it is exercised by hand on the Windows machine once, under the one-launch rules, and its subprocess is injectable for tests)
- [ ] **Step 4: Run, see them pass**
- [ ] **Step 5: Commit** `truth: hook tables and verdicts from a batched Frida differential run`

### Task 12: truth calibration and driver wiring

**Files:**
- Modify: `tools/truth.py`, `tools/driver.py`, `tools/wave.py` (`scorecard_text`), `docs/wso/xrec-report.md`
- Test: `tests/test_tools.py` (`TruthTest`, `DriverTest`)

**Interfaces:**
- Produces: `truth.calibrate(state, rows, results) -> dict` over the 18 ruling rows (10 fork, 8 neither) and a seeded sample of 10 dump-decided `ours` rows (seed recorded): per path, `measured`, `agree`, `disagree`; `Driver` runs `truth` for a batch's ambiguous rows before the worker (oracle path) and once after the worker (Frida path, when `--frida` is on), recording `measured` on each verdict; `--no-frida` default when the calibration measured fewer than a fifth of the sample. The report's scorecard gains the calibration paragraph.

- [ ] **Step 1: Write the failing tests** (`test_truth_calibrate_counts`, `test_driver_records_measured_from_probe_results` with a fake `oracle_run`)
- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run the real calibration**

Run (host side, one launch per scenario, under the 16:45 desktop rules): `python3 -I tools/truth.py calibrate --seed 2026-10-22`
Expected: the table printed and written to the report; the ten sample addresses and the seed in the ledger.

- [ ] **Step 5: Commit** `truth: calibration on the ruling rows, driver records measured verdicts`

### Task 14: ownership claims and a gate lock

Added by thierbig on 2026-10-23. It runs after Task 12 and before Task 13, so that a Claude stream and the GLM driver can work in the same repo independently.

**Files:**
- Create: `tools/claims.py`, `docs/wso/claims.json` (committed, starts as `{}`)
- Modify: `tools/wave.py` (`next_batch`), `tools/gates.sh` (the lock), `docs/wso/README.md` (one paragraph)
- Test: `tests/test_tools.py` (new class `ClaimsTest`)

**Interfaces:**
- Consumes: `wave.load_state` and the assigned batches' files in `docs/wso/wave-state.json`; `wave.save_state`'s atomic write pattern.
- Produces:
  - `docs/wso/claims.json`: a map of stream name to `{holder, since, files: [src paths]}`.
  - `claims.claim(stream, holder, files, claims_path=..., state_path=...)` fails, raising `ClaimError` that names the conflicting holder, when any file is already held by another stream or by an assigned batch in wave-state.json.
  - `claims.release(stream, ...)`.
  - `claims.owned(path, ...) -> str | None`, which returns the holding stream name or the batch id. Assigned batches count as holders.
  - A CLI, `python3 -I tools/claims.py claim <stream> <holder> <file>... | release <stream> | list`. Writes are atomic: a temp file plus `os.replace`.
  - `wave.next_batch` skips files held in claims.json exactly as it skips files owned by assigned batches.
  - `tools/gates.sh` takes an exclusive `flock` on `.gates.lock` at the repo root before running its steps, so a second gate waits. `--no-wait` fails fast with a clear message instead. `.gates.lock` is git-ignored.

- [ ] **Step 1: Write the failing tests:**
  - `test_claim_and_release`;
  - `test_claim_conflicts_with_other_stream`;
  - `test_claim_conflicts_with_assigned_batch`;
  - `test_owned_reports_batches_and_streams`;
  - `test_next_batch_skips_claimed_file`;
  - `test_gate_lock_serializes`: two concurrent invocations of the gate's lock prologue in a temp repo serialise, and `--no-wait` fails fast while the lock is held. Exercise the lock through a dry entry point, not a full gate run.
- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run, see them pass**; then `tools/gates.sh --base iwd2r/main` (the lock is on the gate's own path)
- [ ] **Step 5: README paragraph.** In `docs/wso/README.md`:
  - one writer for wave-state.json, the driver;
  - claim before you edit, release at merge;
  - `git pull --ff-only` before every merge into first;
  - one ledger per plan.
- [ ] **Step 6: Commit** `claims: ownership claims for streams and a lock that serialises gates`

### Task 15: worker pools and a second queue

Added by thierbig on 2026-10-23. It runs after Task 14 and before Task 13. Several model pools can share the batches, and a second driver can work the import list independently of wave 1.

**Files:**
- Modify: `tools/driver.py` (pools), `tools/wave.py` (`run` flags, `--queue`, `scorecard_text` pool column, `next_batch` queue source), `tools/claims.py` (holders from every queue's state file), `docs/wso/README.md` (the "Running the batches" section)
- Test: `tests/test_tools.py` (`DriverTest`, `ClaimsTest`)

**Interfaces:**
- Consumes: `HeadlessWorker(model, max_turns, wrapper)` (Task 6), `Driver` and its park and budget logic (Tasks 8-9), `claims.owned` (Task 14).
- Produces:
  - `Pool = namedtuple('Pool', 'name wrapper model workers')`.
  - `parse_pool(spec: str) -> Pool` for `NAME=WRAPPER:MODEL[:WORKERS]`. WORKERS defaults to 1, and a malformed spec raises `ValueError`. `DEFAULT_POOL = 'glm=claude-glm:glm-5.3-flash[1m]:2'`.
  - `Driver(..., pools=[Pool], workers_by_pool=None, budgets={name: credits})`. Each pool gets its own `HeadlessWorker(model=pool.model, wrapper=pool.wrapper)`; tests inject `workers_by_pool={name: FakeWorker}` instead.
  - The status file holds `status['pools'][name] = {'credits': float, 'budget': float|None, 'parked': {batch_id, at, reason}|None, 'stopped': None|'budget'}`. This replaces Task 9's top-level `status['parked']`, which a run still reads once for migration.
  - `Driver.pick_pool() -> str | None` returns the first pool, in declaration order, that has a free worker (busy < workers), is not parked and is under its budget. When several pools qualify, a cursor that advances after each assignment rotates the choice (round-robin on ties).
  - A worker's 429 parks only its own pool: the WIP is committed and `parked` is set on that pool. The other pools keep taking batches.
  - A pool whose `credits + estimate_next > budget` stops (`stopped: 'budget'`) without affecting the others.
  - `run()` returns 3 when any pool ended parked by a 429, else 0. A later run resumes each pool's parked batch on that pool before cutting new ones.
  - The deciding pool is recorded as `batch['pool'] = name` in the batch's state entry (ruling R10). It is printed as a `pool <name>` line in `ledger_entry` and as a pool column in `scorecard_text`.
  - `wave.py run` flags:
    - `--pool NAME=WRAPPER:MODEL[:WORKERS]` is repeatable; with no `--pool`, the driver uses one pool, `DEFAULT_POOL`.
    - `--budget-NAME C` sets a budget per pool; it is parsed after the pools are known.
    - `--budget C` stays a cap over the sum of all pools' tallies.
    - `--workers N` still sets the default pool's worker count. Combining it with `--pool` is an error, so that Task 13's command line stays valid.
  - `--queue wave1|import` selects the state file through `wave.QUEUES`:
    - `wave1` maps to `docs/wso/wave-state.json`: the `both_changed_differ` rows, with triage applied, and batch ids `b<NNN>`.
    - `import` maps to `docs/wso/import-state.json`: the `theirs_real_ours_stub` rows plus the entries of `docs/wso/import-list.json`, with no triage, and batch ids `i<NNN>`, so branches and worktrees never collide with wave 1 (ruling R11).
    - Both use the same schema.
  - `claims.owned` and `claims.claim` count the assigned batches of every queue's state file as holders; a missing state file holds nothing. As a result, `next_batch` on one queue skips files held by the other queue's assigned batches, and the two queues' files stay disjoint.

- [ ] **Step 1: Write the failing tests** (all with `FakeWorker` and temp state, status and ledger files; never the real wave-state.json):
  - `test_parse_pool_default_and_errors`;
  - `test_two_pools_share_batches`: pools a and b, three batches, assigned a, b, a, each recorded in `batch['pool']`;
  - `test_parked_pool_is_skipped`: a's worker returns `rate_limited` on its first batch; a is parked with the WIP committed, b closes the remaining batches, and `run()` returns 3;
  - `test_pool_budget_stops_only_that_pool`: a's budget is below one batch's credits, so a stops after its first batch and b continues;
  - `test_pool_name_reaches_ledger_entry`: `pool b` appears in the `## driver` entry and in the scorecard;
  - `test_queue_import_uses_its_own_state_and_ids`: a batch cut on `--queue import` lands in import-state.json with id `i001`;
  - `test_queues_keep_files_disjoint`: a file in an assigned wave1 batch is skipped by the import queue's `next_batch`, and `claims.owned` names that batch.
- [ ] **Step 2: Run, see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run, see them pass**; the full tool suite; then `tools/gates.sh --base iwd2r/main`
- [ ] **Step 5: README.** Update "Running the batches":
  - `--pool`, `--budget-NAME` and `--queue`;
  - the per-pool status shape;
  - the fact that a 429 parks one pool only.
- [ ] **Step 6: Commit** `driver: worker pools with per-pool budget and park, a second queue for the import list`

### Task 13: first real batch under watch

**Files:** none new.

- [ ] **Step 1:** `python3 -I tools/wave.py run --workers 1 --budget 4000` on b011 with a human watching the status file and the ledger.
- [ ] **Step 2:** Compare the ledger entry's credits against the Z.ai usage page; record the ratio in the README.
- [ ] **Step 3:** If the batch closes clean, continue with `--workers 2`; if it raised an exception, the coordinator rules and the fix goes into the relevant task before the next batch.
- [ ] **Step 4: Commit** nothing beyond the batch's own close; append the observation to the ledger.

---

## Self-review

- Spec coverage: triage (Tasks 1–4), brief bundles and no-raw-reads rule (Task 5), driver loop, checks, review policy, ledger, budget and park, exceptions (Tasks 6–9), oracle drafts and Frida batch (Tasks 10–11), calibration and the measured column (Task 12, Task 5), first batch under watch (Task 13). Not in scope stays out.
- Step scan: each code step names the function, its file, and the test that pins it; algorithms that the signature does not determine (normaliser token rules, purity regexes, hook conv mapping) are stated in one line each.
- Type consistency: `Result` fields, `parse_report` return shape, `review_needed` tuple and `classify` tuple are used with the same names in later tasks.
- Review Focus items 1–5 are pinned by tests in Tasks 4, 7, 9, 7 and 10 respectively.
- Proportion: no function bodies beyond the test code and the two command lines.
