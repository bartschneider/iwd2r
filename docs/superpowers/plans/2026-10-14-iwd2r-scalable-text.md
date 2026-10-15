# iwd2r Scalable Text Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Crisp, readable text at any UI scale behind `Program Options/Text Renderer=scalable`, with the original bitmap text unchanged by default.

**Architecture:** A text-only render backend captures every `DrawText` call (string, BAM cells, colour, clip, frame position) instead of drawing BAM glyphs; the presenter draws the captured runs after its regions, at window resolution, with stb_truetype glyphs placed in the original cells.

**Tech Stack:** C++17, SDL3 (classic renderer and GPU), stb_truetype (vendored), SIL OFL fonts.

**Spec:** docs/superpowers/specs/2026-10-14-iwd2r-scalable-text-design.md

## Global Constraints

- Default (`Text Renderer` absent or `classic`): no text backend installed; frames byte-identical; the smoke test, presenter goldens and replays unchanged.
- With `scalable`: engine state, layout, rand stream and dumps unchanged; only the presented image changes.
- Decompiled engine code keeps its structure; the only engine edits are the existing render-API hooks' behaviour (no new hooks in decompiled functions unless a DrawText caller needs one, with evidence).
- Third-party code and fonts: stb_truetype (public domain) under `third_party/stb/`, OFL fonts under `third_party/fonts/`, each listed in `docs/THIRD_PARTY.md` with its licence text.
- Engine tests run under `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0`; data tests use data views; `tools/gates.sh --base first` passes with gcc and clang (and `--windows` builds before merge).
- Never open windows on the user's desktop; golden frames come from headless or `--vm` runs.

## Review Focus

1. A string drawn into an off-screen surface that is later blitted must not vanish: it is captured with the blit's offset or kept on the BAM path, never dropped.
2. Layout parity: line breaks, widths and hit tests are identical between `classic` and `scalable` (same BAM metrics).
3. Scale changes mid-run (window resize, Alt+Enter, presenter hotkey) rebuild the glyph atlas; no stale or missing glyphs.
4. Windows-1252 bytes above 0x7F (accents in dialog.tlk) render correctly, not as boxes.
5. The gpu presenter falling back to classic keeps text visible.

---

### Task 1: Inventory and capture

**Files:** Create `src/render/text_capture.h/.cpp`; modify the engine start-up that installs render backends (where `Presenter` is read, `src/main.cpp`); tests in `tests/test_text_capture.cpp`.

- [ ] Inventory every `DrawText` caller (the three hooks: `CVidFont::TextOut` 0x793720, `CVidInf::FXTextOut` 0x79D970, `CVidInf::BKTextOut` 0x79E340) and the surface each draws into; record per caller: back buffer (capture) or off-screen (offset tracking or BAM path). Write it to the stream report.
- [ ] Implement the text-only backend: `DrawText` records `TextRun{resref, text, pos, clip, colour, cells[]}` for back-buffer targets of mapped fonts and returns true; everything else returns false. Cells come from the BAM font's frame sizes and centre points exactly as `CVidFont::TextOut` computes them.
- [ ] Installed only when `Text Renderer=scalable`; `Text Font Scale` parsed.
- [ ] Tests: cells equal the glyph rectangles the BAM path draws, for every mapped font and bytes 0x20-0xFF; nothing installed by default.

### Task 2: Fonts and rasterizer

**Files:** `third_party/stb/stb_truetype.h`, `third_party/fonts/*.ttf` + licences, `platform/text_fonts.h/.cpp`, `platform/text_atlas.h/.cpp`, `docs/THIRD_PARTY.md`; tests.

- [ ] Pick the faces from side-by-side headless screenshots against the original (candidates in the spec); the resref → face + size factor table; unmapped fonts keep BAM.
- [ ] Glyph atlas per (face, pixel size) with stb_truetype, Windows-1252 decoding, outline pass; cache and rebuild on size change.
- [ ] Tests: decoding table, atlas contents for a few glyphs, rebuild on size change.

### Task 3: Presenter overlay

**Files:** `platform/present_backend.h`, `platform/present_classic.cpp`, `platform/present_gpu.cpp`, `platform/present.cpp`, `platform/compose.*` if needed; tests.

- [ ] `DrawText(const TextOverlay&)` after the regions: region assignment, frame-to-window mapping, clip, baseline placement in cells, colour and outline; classic via `SDL_RenderTexture`, gpu via textured quads.
- [ ] Ctrl+Shift+T toggles classic/scalable text live (the backend installs/uninstalls between frames).
- [ ] Tests: mapping at scales 1-4 and both region kinds; golden presented frames (main menu, character screen, a dialog) at Scale 2 and 3 under both presenters, pinned per machine; default byte-identity.

### Task 4: Parity and docs

- [ ] Run every scenario and `replay compare` for chargen_replay and walk_and_talk with `scalable`: identical results.
- [ ] README display section: the two keys, the hotkey, the look; `docs/followups.md`: close the crisp-text entry, record anything left (off-screen callers kept on BAM, proportional fit).
