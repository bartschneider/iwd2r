# iwd2r: crisp scalable text

Status: approved by the user on 2026-10-14 ("write design and i already approve").

## Goal

Text that is crisp and readable at desktop resolutions, at any UI scale, while the original's bitmap text stays the default look. Today every string is drawn from small bitmap fonts (BAM resources such as REALMS, STONESML, TOOLFONT, NORMAL) into the engine's 800x600 UI frame, and the presenter then scales that frame up. Text becomes pixels before scaling, so no filter can make it sharp. The user tried `Scale=2` with the `sharp` filter and rejected it. Full AI-upscaled art (the HD classic project) stays a separate, later project; this design fixes text alone.

## What exists

- **The original already has a TrueType path.** `Program Options/Font Name` (`CBaldurChitin`, `m_sFontName`) makes `CVidInf::BKTextOut` draw through `CVidFont::TextOutEx` with a GDI font instead of the BAM glyphs; the double-byte language builds set it. Scalable text is therefore an extension of an original feature, not a new invention.
- **The render API (project 4) already intercepts every text call before any pixel is written.** `CVidFont::TextOut` (0x793720), `CVidInf::FXTextOut` (0x79D970) and `CVidInf::BKTextOut` (0x79E340) each begin with `if (auto* b = render::Active(); b && b->Clipped(clip).DrawText(font, text, pos, flags)) return TRUE;`. With no backend installed, `Active()` is null and the original drawing runs unchanged. `render::recording` already records `Text{font, text, pos, flags, clip}`.
- **The presenter already maps the frame onto the window per region.** `platform::compose::Region{frame, window, scale, ui}` places each UI panel (or a full-UI screen) at its own scale; `FrameToWindow` maps a frame point to window pixels. Both the classic (SDL renderer) and gpu (SDL3 GPU) backends draw regions through `DrawRegion`.

## Design

### 1. The option

- `Program Options/Text Renderer` (an iwd2r key): `classic` (default) or `scalable`. `classic` changes nothing: no backend is installed for text, every byte drawn is the original's.
- `Program Options/Text Font Scale` (optional, default 1.0): a multiplier on the computed glyph size, for readability preference.
- The Options screen gets no new control in this project; the key is set in the INI or through `play.py --set`. A toggle hotkey (like the presenter's Ctrl+Shift+R) is added for side-by-side comparison: Ctrl+Shift+T.

### 2. Capture: a text-only render backend

With `scalable`, the engine installs a render backend that overrides only `DrawText` (every other hook returns false, so all other drawing stays the original's):

- It records a **text run** for the frame: the string, the font's resref, the position and clip in frame pixels (the render API's `At(..., m_rLockedRect)` already converts surface-relative coordinates), the flags, and the colour the original would draw (the font's realized foreground colour, taken from the same palette the BAM path realizes with `SetColor` and `dwFlags`).
- **Layout parity.** It also records, for each character, the cell the original would draw it in: x advance and glyph box from the BAM font's own frame sizes and centre points (`GetCurrentFrameSize`, `GetCurrentCenterPoint`, the same calls `CVidFont::TextOut` makes). Line breaking, word wrap, widths (`GetStringLength`) and clipping keep using the BAM metrics, so every layout, hit test and replay is unchanged. Only how the glyphs look changes.
- It returns true, so the BAM glyphs are not drawn into the frame and the panel background shows through where the text will be overlaid.
- **Only text drawn to the presented back buffer is captured.** Text drawn into an off-screen surface that is later blitted (if any: to be inventoried, see Risks) keeps the BAM path, with a one-time stderr line naming the caller, so nothing silently disappears.

### 3. Draw: the presenter's text overlay

After the presenter draws a frame's regions, it draws the frame's text runs on top, at window resolution:

- Each run is assigned to the UI region whose frame rectangle contains its position; its scale and offset come from that region (`FrameToWindow`). Runs in the world region (floating text over creatures, if any) use the world region's scale.
- Each character is drawn inside its recorded cell, mapped to window pixels: the TrueType glyph is sized so its cap height matches the BAM font's cell height times the region scale (times `Text Font Scale`), placed on the cell's baseline, left-aligned in the cell. Cells guarantee no glyph overflows its line or its clip; the clip is mapped to window pixels and applied.
- Colour: the run's recorded colour, with a one-window-pixel dark outline matching the original's dark glyph border, so text stays readable over painted backgrounds.
- Rasterizer: `stb_truetype` (public domain, single header) vendored in `third_party/stb/`, building a glyph atlas per (face, pixel size), cached across frames and rebuilt when the window or scale changes. No FreeType or HarfBuzz dependency.
- Both presenters get it: classic draws the atlas with `SDL_RenderTexture` per glyph (alpha blended); gpu draws textured quads from the same atlas. A new backend method `DrawText(const TextOverlay&)` runs after the regions and before `EndFrame`.

### 4. Fonts

- Each BAM font resref maps to a bundled TrueType face and a size factor, in one table (`platform/text_fonts.cpp`): body text (REALMS, NORMAL) to a readable book serif, headings and buttons (STONESML, STONESM2, STONEBIG) to a display serif in the original's carved style, small labels (TOOLFONT) to a clear sans. Fonts with no mapping (INITIALS drop caps, symbol fonts such as STATES2, WMDAG) keep the BAM path.
- Faces are SIL Open Font License fonts, committed under `third_party/fonts/` with their licences listed in `docs/THIRD_PARTY.md`. The exact faces are picked in the implementation plan from side-by-side screenshots against the original; a candidate set is EB Garamond or Crimson Pro (body), Cinzel (headings) and Source Sans 3 (small labels).
- Character set: the game's text is Windows-1252 (the `dialog.tlk` strings); the overlay decodes it to Unicode code points. Double-byte languages keep their existing Font Name path; the overlay is off when `Double Byte Character Support` is set.

### 5. What does not change

- Default runs (`Text Renderer=classic`): byte-identical frames; the smoke test, presenter golden frames and every replay are unaffected.
- With `scalable`: the engine's state, rand stream, layout and dumps are identical; the frame (`NAME.png`) lacks the text, and the presented image (`NAME-presented.png`) has it.

## Testing

- **Unit:** the capture records the same cells as the BAM path draws (compare cell boxes with the glyph rectangles `CVidFont::TextOut` renders, for every mapped font and the full Windows-1252 range); the region assignment and frame-to-window mapping at scales 1 to 4; Windows-1252 decoding; the clip.
- **Golden:** `--shot-presented` frames of the main menu, the character screen and a dialog at Scale 2 and 3 under both presenters, pinned per machine like the smoke hash, and inspected once by eye.
- **Parity:** with `scalable`, every existing scenario runs and the `replay compare` results are unchanged.
- **Default:** a test asserts no backend is installed and frames are byte-identical when the key is absent.

## Risks

- **Off-screen text targets.** Some controls may draw text into a cached surface (scrolling dialog, journal) that is later blitted to the back buffer; those runs would need the blit's offset to be tracked. The plan's first task inventories every DrawText caller's target surface and decides per caller: capture with a tracked offset, or keep BAM.
- **Look.** A modern face can feel wrong next to painted panels. Mitigation: the side-by-side font pick, the outline, `Text Font Scale`, and the hotkey for comparison.
- **Fit.** TrueType glyphs drawn into BAM cells can look slightly uneven in spacing, because each character keeps the bitmap font's width. Accepted for parity; a later option may fit runs proportionally inside the same total width.

## Out of scope

AI-upscaled art (HD classic); an Options-screen control for the text renderer; double-byte languages; changing any layout to take advantage of crisper text.
