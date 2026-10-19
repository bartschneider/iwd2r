# GL trace: world_entry

Captured 20261018-2 by tools/gltrace.py (c15aaba6), summary of `apitrace 14.0 dump`.

- game: /mnt/c/Users/thier/iwd2r-ieex/game (IWD2EE.exe sha256 0c6d1dde22dcd53a)
- apitrace: 14.0 (captured by its lib/wrappers/opengl32.dll proxy in the game folder, writing $TRACE_FILE)
- GPU: Parsec Virtual Display Adapter
- scenario: scenarios/world_entry.txt, driven to its last checkpoint (waits are wall-clock; 50 s lead for the intro movies)
- frames: 11873
- draw calls: 0 total (mean 0.0/frame, max 0)
- texture uploads (glTexImage2D): 11878 total
  - 256x256 GL_RGBA: 2
  - 512x512 GL_RGBA: 1
  - 640x320 GL_RGBA8: 4299
  - 640x480 GL_RGBA8: 7573
  - 720x62 GL_RGBA8: 1
  - 720x90 GL_RGBA8: 1
  - 800x600 GL_RGBA8: 1
- FBO binds (glBindFramebuffer): 0
- distinct texture dimensions: 256x256, 512x512, 640x320, 640x480, 720x62, 720x90, 800x600

A frame is the calls from after the previous buffer swap through the swap; calls after the last swap are no frame.

| frame | draws | fbo binds | uploads |
|---|---|---|---|
| 1 | 0 | 0 | 3 |
| 2 | 0 | 0 | 1 |
| 3 | 0 | 0 | 1 |
| 4 | 0 | 0 | 1 |
| 5 | 0 | 0 | 1 |
| 6 | 0 | 0 | 1 |
| 7 | 0 | 0 | 1 |
| 8 | 0 | 0 | 1 |
| 9 | 0 | 0 | 1 |
| 10 | 0 | 0 | 1 |
| 11 | 0 | 0 | 1 |
| 12 | 0 | 0 | 1 |
| 13 | 0 | 0 | 1 |
| 14 | 0 | 0 | 1 |
| 15 | 0 | 0 | 1 |
| 16 | 0 | 0 | 1 |
| 17 | 0 | 0 | 1 |
| 18 | 0 | 0 | 1 |
| 19 | 0 | 0 | 1 |
| 20 | 0 | 0 | 1 |
| 21 | 0 | 0 | 1 |
| 22 | 0 | 0 | 1 |
| 23 | 0 | 0 | 1 |
| 24 | 0 | 0 | 1 |
| 25 | 0 | 0 | 1 |
| 26 | 0 | 0 | 1 |
| 27 | 0 | 0 | 1 |
| 28 | 0 | 0 | 1 |
| 29 | 0 | 0 | 1 |
| 30 | 0 | 0 | 1 |
| 31 | 0 | 0 | 1 |
| 32 | 0 | 0 | 1 |
| 33 | 0 | 0 | 1 |
| 34 | 0 | 0 | 1 |
| 35 | 0 | 0 | 1 |
| 36 | 0 | 0 | 1 |
| 37 | 0 | 0 | 1 |
| 38 | 0 | 0 | 1 |
| 39 | 0 | 0 | 1 |
| 40 | 0 | 0 | 1 |

11873 frames total; the full per-frame table is in runs/20261018-203206/world_entry-frames.json (not committed).
