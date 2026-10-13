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

- FSR: EASU's four `textureGather` reads are twelve `texelFetch` reads clamped to the presenter region's frame rectangle. RCAS reads EASU's output clamped to the region's pixels, guards its two limiter divisions against zero (the original relies on the hardware's NaN handling in `max`), and resolves with an exact reciprocal instead of `APrxMedRcpF1`, so that sharpness 0 leaves the pixels unchanged. The optional denoise and alpha paths are left out.
- NIS: the compute shader's groupshared luma tile and edge map are computed per output pixel in a fragment shader, with the same arithmetic. Only the SDR, 32-bit, non-viewport, non-NV12 path is ported. The coefficient tables are pushed as a uniform block instead of read from textures.
