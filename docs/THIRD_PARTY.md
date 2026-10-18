# Third-party code

The GPU presenter's upscaling filters are ports of two MIT-licensed shaders. Only the parts the fragment passes need were ported; the SDKs are not vendored.

| Our files | Ported from | Copyright |
|---|---|---|
| `platform/shaders/fsr_easu.frag`, `platform/shaders/fsr_rcas.frag` | AMD FidelityFX Super Resolution 1.0, `ffx_fsr1.h` (v1.20210629) and the approximations it uses from `ffx_a.h`, [GPUOpen-Effects/FidelityFX-FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR) | Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. |
| `platform/shaders/nis.frag`, `platform/present_nis.cpp` | NVIDIA Image Scaling SDK v1.0.3, `NIS_Scaler.h` (NVScaler) and `NIS_Config.h` (`NVScalerUpdateConfig`, `coef_scale`, `coef_usm`), [NVIDIAGameWorks/NVIDIAImageScaling](https://github.com/NVIDIAGameWorks/NVIDIAImageScaling) | Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved. |

Both are distributed under the MIT License:

```
The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

Changes from the originals:

- FSR: EASU's four `textureGather` reads are twelve `texelFetch` reads clamped to the presenter region's frame rectangle. RCAS reads EASU's output clamped to the region's pixels. Its two limiter divisions are kept off zero and a select stands in for the hardware `max` that the original relies on to drop the NaN they make where a ring channel is all 0 or all 1, so the lobe is the original's. The one arithmetic difference: the resolve uses an exact reciprocal instead of `APrxMedRcpF1` (which is 0.3% low at lobe 0), so that sharpness 0 leaves EASU's pixels unchanged. The optional denoise and alpha paths are left out.
- NIS: the compute shader's groupshared luma tile and edge map are computed per output pixel in a fragment shader, with the same arithmetic. Only the SDR, 32-bit, non-viewport, non-NV12 path is ported. The coefficient tables are pushed as a uniform block instead of read from textures.

## Scalable text

`Program Options/Text Renderer=scalable` (platform/text_*.cpp) rasterizes TrueType faces with stb_truetype and embeds four SIL Open Font License faces in the executable.

| Our files | From | Licence |
|---|---|---|
| `third_party/stb/stb_truetype.h` | stb_truetype v1.26, [nothings/stb](https://github.com/nothings/stb) at commit 6e9f34d5429cf16790ec43c9bac3f1ee4ad1f760 (SHA-256 ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab), unmodified | Public domain (Unlicense), or MIT, at the user's choice; the text is at the end of the file |
| `third_party/fonts/CrimsonPro-Bold.ttf` | Crimson Pro Bold, [Fonthausen/CrimsonPro](https://github.com/Fonthausen/CrimsonPro) `fonts/ttf/` at commit 475ca6a0469ba2bd9c2a3bbf038957099392c04d, unmodified | SIL OFL 1.1, `third_party/fonts/OFL-CrimsonPro.txt`; Copyright 2018 The Crimson Pro Project Authors |
| `third_party/fonts/Marcellus-Regular.ttf` | Marcellus, [google/fonts](https://github.com/google/fonts) `ofl/marcellus/`, unmodified | SIL OFL 1.1, `third_party/fonts/OFL-Marcellus.txt`; Copyright (c) 2012, Brian J. Bonislawsky DBA Astigmatic (AOETI), Reserved Font Name "Marcellus" |
| `third_party/fonts/MarcellusSC-Regular.ttf` | Marcellus SC, [google/fonts](https://github.com/google/fonts) `ofl/marcellussc/`, unmodified | SIL OFL 1.1, `third_party/fonts/OFL-MarcellusSC.txt`; Copyright (c) 2012, Brian J. Bonislawsky DBA Astigmatic (AOETI), Reserved Font Name "Marcellus" |
| `third_party/fonts/UncialAntiqua-Regular.ttf` | Uncial Antiqua, [google/fonts](https://github.com/google/fonts) `ofl/uncialantiqua/`, unmodified | SIL OFL 1.1, `third_party/fonts/OFL-UncialAntiqua.txt`; Copyright (c) 2011 by Brian J. Bonislawsky DBA Astigmatic (AOETI), Reserved Font Name "Uncial Antiqua" |

The fonts are embedded unmodified under their original names (the OFL allows bundling them with software; the licence files travel with them in the source tree). SHA-256: CrimsonPro-Bold.ttf d8fc7d47b2b8681e7513e365764808b986999e1a78928cc58d04f237d8b8f798, Marcellus-Regular.ttf 1cf0cd10b17d35e852729962cc1ffaffed94514895972458345e2df34abb2f81, MarcellusSC-Regular.ttf 03044ab65228b0554c9a5c1e63df29d4e58a71857e212801aaf768dfd106bdf0, UncialAntiqua-Regular.ttf 33a5128b59d1c95d4f3788164f4ab3a1196f0a982263cb2cd278c47418366766.

## Recovered engine code from the WSO fork

Engine functions recovered by the parallel effort at [WillScarlettOhara/iwd2-re](https://github.com/WillScarlettOhara/iwd2-re) (the WSO fork) are imported behind the evidence gate, per function or as whole files where we lack the file; [docs/wso/README.md](wso/README.md) describes the project and the import rules. Each import commit marks the functions it brings with the trailer:

```
Imported-From: WillScarlettOhara/iwd2-re@<40-char sha> (MIT per author, 2026-10-17)
```

| Our files | Ported from | Copyright |
|---|---|---|
| Functions carrying the `Imported-From` trailer | [WillScarlettOhara/iwd2-re](https://github.com/WillScarlettOhara/iwd2-re), recovered engine functions | Copyright the fork's author (WillScarlettOhara). MIT, as granted to Thierry on 2026-10-17; the link to his LICENSE goes here when it is published. |

The additions are MIT, under the standard MIT text quoted at the top of this file. Nothing from the fork's IWD2EE branch — game-data overrides, compiled binaries — is vendored; the renderer artifacts are fetched by pinned hash outside the repository (docs/wso/README.md).

## Differential Frida tracing

The differential-tracing toolchain (docs/frida-tracing.md) is adopted from the same fork, at commit e8dd16a750966af53f24daaf49dcb4fb13594c13, under the same MIT grant.

| Our files | Ported from | Copyright |
|---|---|---|
| `tools/frida/hooks.py` | `scripts/frida_hooks.py` (the readers, argument expressions, prelude and render functions) | Copyright the fork's author (WillScarlettOhara). MIT, as granted to Thierry on 2026-10-17 |
| `tools/frida/orig.py` | `scripts/frida_orig.py` (Trace/verdict shape, hook wiring) with the exception guard from his `scripts/frida_crash_guard.py` | as above |
| `tools/frida/trace_diff.py` | `scripts/frida_trace_diff.py` (CLI and comparison machinery) | as above |
| `tools/frida/probe.py` | `scripts/frida_probe.py` (the two-runtime shape), rewritten for our sandbox pipeline | as above |
| `docs/frida-tracing.md` | his `docs/frida-trace-comparison.md` and `docs/frida-differential-tracing.md` (the method and the gotchas), rewritten for our paths | as above |

Dropped on purpose: the fork's in-target load driver (cursor calibration, movie skip, the click driver — the house pipeline drives scenarios), and his PROCESS-EXITED verdict (tools/frida/orig.py explains why). Our additions: `rva` hook sites for our rebuild-moving build, the sandbox staging in probe.py, the in-memory compare wrappers.

## Structural parity gates

The structural-parity gate itself — tools/parity/run.py (the changed-functions table, verdicts and exports plumbing), tools/parity/setup.sh, requirements.txt and tools/ghidra/ExportCallGraph.java — is ours. Its checks are adopted from the same fork, at commit e8dd16a750966af53f24daaf49dcb4fb13594c13, under the same MIT grant; each file's docstring names its deviations (the fork's Ghidra-bridge and VM dependencies are rehosted on this repo's exports and oracle exe).

| Our files | Ported from | Copyright |
|---|---|---|
| `tools/parity/asm_verify.py` | `scripts/reagent_asm_verify.py` (the body/callee diffing) | Copyright the fork's author (WillScarlettOhara). MIT, as granted to Thierry on 2026-10-17 |
| `tools/parity/address_markers.py` | `scripts/lint_address_markers.py` (the marker checks; the TSV export reader is ours) | as above |
| `tools/parity/re_lint.py` | `scripts/re_lint.py` (rules and CLI; the per-function entry is ours) | as above |
| `tools/parity/lint_extend_cells.py` | `scripts/lint_extend_cells.py` | as above |
| `tools/parity/lint_infinite_loop.py` | `scripts/lint_infinite_loop.py` | as above |
| `tools/parity/lint_pragma_pack_balance.py` | `scripts/lint_pragma_pack_balance.py` | as above |
| `tools/parity/lint_stuck_loop_index.py` | `scripts/lint_stuck_loop_index.py` | as above |
| `tools/parity/lint_twin_symmetry.py` | `scripts/lint_twin_symmetry.py` | as above |
| `tools/parity/lint_uninit_member.py` | `scripts/lint_uninit_member.py` | as above |
| `tools/parity/vtable_audit.py` | `scripts/vtable_audit.py` (the vtable location and slot-diff audit) | as above |
| `tools/parity/ctor_vtable_check.py` | `scripts/ctor_vtable_check.py` | as above |
| `tools/parity/struct_layout_audit.py` | `scripts/struct_layout_audit.py` (the `/* 0xNNN */` layout audit) | as above |
| `tools/parity/parity_offsets.py` | `scripts/parity_offsets.py` (the thiscall member audit; his `scripts/sym.py`/`scripts/src_find.py` dependencies rehosted on direct capstone disassembly and marker scans) | as above |


