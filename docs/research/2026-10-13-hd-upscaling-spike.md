# HD classic: local upscaling spike (2026-10-13)

A throwaway feasibility probe for the future HD-classic project: AI-upscaling the player's own game art locally, on the player's machine, into a cache. Nothing derived from game art is shipped or committed. The probe's scripts were not kept; this note records what it found.

## Verdict

Practical, with two conditions: upscaling runs as a background job that does the UI and the current area first rather than a blocking whole-install pass, and character sprites stay palette-indexed after upscaling instead of becoming RGB.

## Decoding

About 200 lines of Python decoded everything from the vanilla copy through `tools/bif.py` plus: raw KEY locators for tileset entries (bits 14–19 and the 20-byte tileset table), BIFC V1.0 decompression (every area BIF under `CD2/Data/AR*.bif` is compressed; `bif.py` refuses these today and the project's own resource code must handle them), BAM V1 and BAMC (RLE on when bit 31 is clear; only the transparent index is run-length encoded), TIS V1 (no PVRZ in IWD2), WED V1.3 overlay 0 for laying tiles out, MOS V1 and MOSC (one palette per 64 px block). The start area is AR1000 (`STARTARE.2DA`).

## Size of the job

| Kind | Files | Units | Source pixels |
|---|---|---|---|
| BAM | 11,366 | 1.56M frame entries, 802k unique frames | 2.19 Gpx |
| TIS | 105 | 104,828 tiles | 0.43 Gpx |
| MOS | 284 | 11,482 blocks | 0.04 Gpx |

About 75% of BAM frames are 64 px or smaller, so the upscaler must work on packed atlases. A full cache is about 13–15 GB of PNG at 2x and 50–55 GB at 4x (about 43 GB as BC7).

## Throughput (`realesrgan-ncnn-vulkan`, run from WSL through cmd.exe)

| Path | Fast model 2x | Fast model 4x | Anime 4x | General 4x |
|---|---|---|---|---|
| RTX 5070 Ti, AR1000 (7.4 Mpx) | 2.8 s | 7.8 s | 11.0 s | 20.9 s |
| RTX 5070 Ti, whole install | ~17 min | ~47 min | ~66 min | ~2.1 h |
| AMD integrated GPU | ~3.5 h | ~3.5 h | out of memory | out of memory |
| WSL CPU (llvmpipe) | ~6 h | — | — | days |

Everything except BAMs (TIS, MOS, GUI; 0.47 Gpx) takes about 3 min at 2x or 22 min at 4x with the general model on the RTX. WSL has no GPU Vulkan. The NVIDIA path loses the device on RGBA input (`vkWaitForFences -4`) and then writes blank files; RGB input with transparent pixels filled from the nearest opaque colour is fine. The AMD GPU runs out of memory on the larger models. Neither failure changes the exit code, so every output must be verified (exists, right size, not blank).

Quality: the general model is best for areas (the anime model cel-shades wood); large monsters are good; 1 px UI borders halo; 29×42 character sprites mostly get smoothed, there are too few pixels to add detail.

## The palette problem

Character BAMs use a false-colour key palette that the engine remaps at runtime (index 0 transparent, index 1 shadow, seven 12-entry colour ranges from index 4). Measured against the ideal (upscaling the already-remapped sprite), mean absolute error:

| Approach | Error | Note |
|---|---|---|
| Original remapped, nearest 4x | 24.0 | baseline |
| (a) Lanczos per-index masks, re-indexed | 24.2 | remaps perfectly, adds no detail, CPU only |
| (a') AI-upscaled masks | 48.0 | holes and slivers |
| (b) AI RGB, nearest palette entry, remap by luminance ratio | 25.7 | keeps detail, some speckle at boundaries |
| (b2) zones from (a), shade from the AI image | 25.1 | best cheap option |
| AI per palette combination | 0 | ideal, one pass per BAM per palette |

Recommendation: (b2), stored as an 8-bit index plane plus a detail/alpha plane plus a shadow alpha; the shader draws `palette[idx] * detail`, so all runtime remaps and palette effects keep working from one upscale per BAM. Optionally re-upscale the current party's real palettes in the background and swap them in. Art the engine never remaps is stored as RGBA.

## Architecture recommendation

- A separate worker process: resolve resources from KEY plus Override; pack BAM frames into atlases with an edge-extended apron of 8 px or more, RGB only; upscale a TIS as the whole WED-assembled area (door and secondary tiles included) and cut it back into tiles; decode a MOS to a full image first; take alpha and shadow from Lanczos-upscaled coverage masks; choose the GPU explicitly; verify every output.
- Cache: `<cache>/iwd2r/hd/v<pipeline>/<model>-x<N>/{BAM,TIS,MOS}/<RESREF>.<srcsha>/` with `meta.json` and texture pages, keyed by the source bytes after Override, the pipeline version, the model hash, the scale and the tile size (tile size changes the output); TIS keys include the WED hash; entries written atomically.
- When: opt-in; a short progress screen does the GUI art first (1–5 min on a discrete GPU), then a priority queue works through the current area, its creatures, neighbouring areas, then the rest while idle. The renderer draws the original at N× and hot-swaps when the HD version lands; a CLI mode pre-bakes everything.
- Renderer contract (project 4's API): textures at N× (RGBA or index-plus-detail); every position, size, centre and hit test stays in original logical coordinates; draw calls take a logical rect plus a texture rect; N is per texture, not global.
- What breaks: BAM content bleeds past the frame box (keep the apron and store the frame rect); animations shimmer between frames; weapon layers (`ANMweap`) must use the same model and scale as their bodies; per-tile TIS upscaling shows seams about 3× the interior error (13.9 vs 4.6), whole-area upscaling brings it to 5.9; MOS per-block palettes seam the same way; fonts are never AI-upscaled; pixel-exact UI chrome needs per-resource exclusion and a nearest or xBR mode; additive spell effects (VVC) need a black fill, not nearest-colour; search, height and light maps stay at original resolution.
