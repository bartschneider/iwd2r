# GL trace: first_fight

Captured 20261018 by tools/gltrace.py (1f372d27), summary of `apitrace 14.0 dump`.

- game: /mnt/c/Users/thier/iwd2r-ieex/game (IWD2EE.exe sha256 0c6d1dde22dcd53a)
- apitrace: 14.0 (captured by its lib/wrappers/opengl32.dll proxy in the game folder, writing $TRACE_FILE)
- GPU: Parsec Virtual Display Adapter
- scenario: scenarios/first_fight.txt, driven to its last checkpoint (waits are wall-clock; 50 s lead for the intro movies)
- frames: 67288
- draw calls: 303169 total (mean 4.5/frame, max 226)
- texture uploads (glTexImage2D + glTexSubImage2D): 232053 total
  - 128x128 (subimage: internal format n/a): 109612
  - 256x256 GL_RGBA: 2
  - 2x62 (subimage: internal format n/a): 20
  - 312x60 (subimage: internal format n/a): 20
  - 314x2 (subimage: internal format n/a): 20
  - 512x512 GL_RGBA: 1
  - 640x320 GL_RGBA8: 4306
  - 640x480 GL_RGBA8: 8175
  - 64x64 (subimage: internal format n/a): 280
  - 720x62 GL_RGBA8: 1
  - 720x90 GL_RGBA8: 1
  - 800x600 GL_RGBA8: 1
  - 96x96 (subimage: internal format n/a): 109614
- FBO binds (glBindFramebuffer + -EXT): 214347
- distinct texture dimensions: 128x128, 256x256, 2x62, 312x60, 314x2, 512x512, 640x320, 640x480, 64x64, 720x62, 720x90, 800x600, 96x96

A frame is the calls from after the previous buffer swap through the swap; calls after the last swap are no frame. Draw calls are vertex-array draws and immediate-mode glBegin blocks; FBO binds are glBindFramebuffer(-EXT); glTexSubImage2D carries no internal format, so its uploads have none.

| frame | draws | fbo binds | uploads |
|---|---|---|---|
| 1 | 2 | 6 | 3 |
| 2 | 2 | 4 | 1 |
| 3 | 2 | 4 | 1 |
| 4 | 2 | 4 | 1 |
| 5 | 2 | 4 | 1 |
| 6 | 2 | 4 | 1 |
| 7 | 2 | 4 | 1 |
| 8 | 2 | 4 | 1 |
| 9 | 2 | 4 | 1 |
| 10 | 2 | 4 | 1 |
| 11 | 2 | 4 | 1 |
| 12 | 2 | 4 | 1 |
| 13 | 2 | 4 | 1 |
| 14 | 2 | 4 | 1 |
| 15 | 2 | 4 | 1 |
| 16 | 2 | 4 | 1 |
| 17 | 2 | 4 | 1 |
| 18 | 2 | 4 | 1 |
| 19 | 2 | 4 | 1 |
| 20 | 2 | 4 | 1 |
| 21 | 2 | 4 | 1 |
| 22 | 2 | 4 | 1 |
| 23 | 2 | 4 | 1 |
| 24 | 2 | 4 | 1 |
| 25 | 2 | 4 | 1 |
| 26 | 2 | 4 | 1 |
| 27 | 2 | 4 | 1 |
| 28 | 2 | 4 | 1 |
| 29 | 2 | 4 | 1 |
| 30 | 2 | 4 | 1 |
| 31 | 2 | 4 | 1 |
| 32 | 2 | 4 | 1 |
| 33 | 2 | 4 | 1 |
| 34 | 2 | 4 | 1 |
| 35 | 2 | 4 | 1 |
| 36 | 2 | 4 | 1 |
| 37 | 2 | 4 | 1 |
| 38 | 2 | 4 | 1 |
| 39 | 2 | 4 | 1 |
| 40 | 2 | 4 | 1 |

67288 frames total; the full per-frame table is in runs/20261018-214947/first_fight-frames.json (not committed).
