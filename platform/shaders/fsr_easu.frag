#version 450

// AMD FidelityFX Super Resolution 1.0, EASU (edge adaptive spatial
// upsampling): FsrEasuF from ffx_fsr1.h v1.20210629, 32-bit path.
// Copyright (c) 2021 Advanced Micro Devices, Inc. MIT licence; see
// docs/THIRD_PARTY.md.
//
// Ported as a fragment shader for the presenter's regions: the 12 taps are
// texelFetch reads clamped to the region's frame rectangle (in place of
// FSR's four gathers), so nothing bleeds in from a neighbouring region. The
// output goes to an intermediate texture the size of the output; RCAS
// (fsr_rcas.frag) sharpens it into the target.
layout(set = 2, binding = 0) uniform sampler2D frameTexture;

layout(set = 3, binding = 0) uniform Region {
    vec4 frameRect; // left, top, right, bottom in texels
    vec4 dest; // destination origin x, y in output pixels; scale x, y (output pixels per texel)
};

layout(location = 0) out vec4 color;

// ffx_a.h's approximations, bit for bit.
float APrxLoRcpF1(float a) { return uintBitsToFloat(uint(0x7ef07ebb) - floatBitsToUint(a)); }
float APrxLoRsqF1(float a) { return uintBitsToFloat(uint(0x5f347d74) - (floatBitsToUint(a) >> uint(1))); }

vec3 Tap(vec2 f, vec2 off)
{
    ivec2 t = clamp(ivec2(f + off), ivec2(frameRect.xy), ivec2(frameRect.zw) - 1);
    return texelFetch(frameTexture, t, 0).rgb;
}

// Filtering for a given tap.
void FsrEasuTapF(inout vec3 aC, inout float aW, vec2 off, vec2 dir, vec2 len, float lob, float clp, vec3 c)
{
    // Rotate offset by direction.
    vec2 v;
    v.x = (off.x * (dir.x)) + (off.y * dir.y);
    v.y = (off.x * (-dir.y)) + (off.y * dir.x);
    // Anisotropy.
    v *= len;
    // Compute distance^2, limited to the window.
    float d2 = v.x * v.x + v.y * v.y;
    d2 = min(d2, clp);
    // Approximation of lanczos2 without sin() or rcp(), or sqrt() to get x.
    float wB = float(2.0 / 5.0) * d2 + float(-1.0);
    float wA = lob * d2 + float(-1.0);
    wB *= wB;
    wA *= wA;
    wB = float(25.0 / 16.0) * wB + float(-(25.0 / 16.0 - 1.0));
    float w = wB * wA;
    aC += c * w;
    aW += w;
}

// Accumulate direction and length for one of the four bilinear corners.
void FsrEasuSetF(inout vec2 dir, inout float len, float w, float lA, float lB, float lC, float lD, float lE)
{
    float dc = lD - lC;
    float cb = lC - lB;
    float lenX = max(abs(dc), abs(cb));
    lenX = APrxLoRcpF1(lenX);
    float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = clamp(abs(dirX) * lenX, 0.0, 1.0);
    lenX *= lenX;
    len += lenX * w;
    float ec = lE - lC;
    float ca = lC - lA;
    float lenY = max(abs(ec), abs(ca));
    lenY = APrxLoRcpF1(lenY);
    float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = clamp(abs(dirY) * lenY, 0.0, 1.0);
    lenY *= lenY;
    len += lenY * w;
}

float Luma2(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }

void main()
{
    // FSR's pp = ip * con0.xy + con0.zw: the output pixel centre in input
    // texel space with texel k's centre at k.
    vec2 pp = frameRect.xy + (gl_FragCoord.xy - dest.xy) / dest.zw - 0.5;
    vec2 fp = floor(pp);
    pp -= fp;

    // 12-tap kernel.
    //    b c
    //  e f g h
    //  i j k l
    //    n o
    vec3 b = Tap(fp, vec2(0.0, -1.0));
    vec3 c = Tap(fp, vec2(1.0, -1.0));
    vec3 e = Tap(fp, vec2(-1.0, 0.0));
    vec3 f = Tap(fp, vec2(0.0, 0.0));
    vec3 g = Tap(fp, vec2(1.0, 0.0));
    vec3 h = Tap(fp, vec2(2.0, 0.0));
    vec3 i = Tap(fp, vec2(-1.0, 1.0));
    vec3 j = Tap(fp, vec2(0.0, 1.0));
    vec3 k = Tap(fp, vec2(1.0, 1.0));
    vec3 l = Tap(fp, vec2(2.0, 1.0));
    vec3 n = Tap(fp, vec2(0.0, 2.0));
    vec3 o = Tap(fp, vec2(1.0, 2.0));

    // Simplest multi-channel approximate luma possible (luma times 2).
    float bL = Luma2(b);
    float cL = Luma2(c);
    float eL = Luma2(e);
    float fL = Luma2(f);
    float gL = Luma2(g);
    float hL = Luma2(h);
    float iL = Luma2(i);
    float jL = Luma2(j);
    float kL = Luma2(k);
    float lL = Luma2(l);
    float nL = Luma2(n);
    float oL = Luma2(o);

    // Accumulate for bilinear interpolation.
    vec2 dir = vec2(0.0);
    float len = 0.0;
    FsrEasuSetF(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);
    FsrEasuSetF(dir, len, pp.x * (1.0 - pp.y), cL, fL, gL, hL, kL);
    FsrEasuSetF(dir, len, (1.0 - pp.x) * pp.y, fL, iL, jL, kL, nL);
    FsrEasuSetF(dir, len, pp.x * pp.y, gL, jL, kL, lL, oL);

    // Normalize with approximation, and cleanup close to zero.
    vec2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    bool zro = dirR < float(1.0 / 32768.0);
    dirR = APrxLoRsqF1(dirR);
    dirR = zro ? 1.0 : dirR;
    dir.x = zro ? 1.0 : dir.x;
    dir *= vec2(dirR);
    // Transform from {0 to 2} to {0 to 1} range, and shape with square.
    len = len * 0.5;
    len *= len;
    // Stretch kernel {1.0 vert|horz, to sqrt(2.0) on diagonal}.
    float stretch = (dir.x * dir.x + dir.y * dir.y) * APrxLoRcpF1(max(abs(dir.x), abs(dir.y)));
    // Anisotropic length after rotation.
    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 + -0.5 * len);
    // The window shifts from +/-{sqrt(2.0) to slightly beyond 2.0}.
    float lob = 0.5 + float((1.0 / 4.0 - 0.04) - 0.5) * len;
    // Set distance^2 clipping point to the end of the adjustable window.
    float clp = APrxLoRcpF1(lob);

    // Accumulation mixed with min/max of 4 nearest.
    vec3 min4 = min(min(f, min(g, j)), k);
    vec3 max4 = max(max(f, max(g, j)), k);
    vec3 aC = vec3(0.0);
    float aW = 0.0;
    FsrEasuTapF(aC, aW, vec2(0.0, -1.0) - pp, dir, len2, lob, clp, b);
    FsrEasuTapF(aC, aW, vec2(1.0, -1.0) - pp, dir, len2, lob, clp, c);
    FsrEasuTapF(aC, aW, vec2(-1.0, 1.0) - pp, dir, len2, lob, clp, i);
    FsrEasuTapF(aC, aW, vec2(0.0, 1.0) - pp, dir, len2, lob, clp, j);
    FsrEasuTapF(aC, aW, vec2(0.0, 0.0) - pp, dir, len2, lob, clp, f);
    FsrEasuTapF(aC, aW, vec2(-1.0, 0.0) - pp, dir, len2, lob, clp, e);
    FsrEasuTapF(aC, aW, vec2(1.0, 1.0) - pp, dir, len2, lob, clp, k);
    FsrEasuTapF(aC, aW, vec2(2.0, 1.0) - pp, dir, len2, lob, clp, l);
    FsrEasuTapF(aC, aW, vec2(2.0, 0.0) - pp, dir, len2, lob, clp, h);
    FsrEasuTapF(aC, aW, vec2(1.0, 0.0) - pp, dir, len2, lob, clp, g);
    FsrEasuTapF(aC, aW, vec2(1.0, 2.0) - pp, dir, len2, lob, clp, o);
    FsrEasuTapF(aC, aW, vec2(0.0, 2.0) - pp, dir, len2, lob, clp, n);
    // Normalize and dering.
    color = vec4(min(max4, max(min4, aC * vec3(1.0 / aW))), 1.0);
}
