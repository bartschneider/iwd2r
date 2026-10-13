#version 450

// AMD FidelityFX Super Resolution 1.0, RCAS (robust contrast adaptive
// sharpening): FsrRcasF from ffx_fsr1.h v1.20210629, 32-bit path, without
// the optional denoise and alpha pass-through.
// Copyright (c) 2021 Advanced Micro Devices, Inc. MIT licence; see
// docs/THIRD_PARTY.md.
//
// Reads EASU's output (an intermediate texture the size of the output) at
// this output pixel and its four neighbours, clamped to the region's
// scissor rectangle so a region never sharpens against its neighbour.
layout(set = 2, binding = 0) uniform sampler2D easuTexture;

layout(set = 3, binding = 0) uniform Rcas {
    vec4 clipRect; // left, top, right, bottom of the region in output pixels
    vec4 params; // x: sharpness as a linear amount, FsrRcasCon's con.x (exp2(-stops))
};

layout(location = 0) out vec4 color;

// This is set at the limit of providing unnatural results for sharpening.
#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

vec3 Load(ivec2 p)
{
    ivec2 t = clamp(p, ivec2(clipRect.xy), ivec2(clipRect.zw) - 1);
    return texelFetch(easuTexture, t, 0).rgb;
}

float Min3(float x, float y, float z) { return min(x, min(y, z)); }
float Max3(float x, float y, float z) { return max(x, max(y, z)); }

void main()
{
    // Algorithm uses minimal 3x3 pixel neighborhood.
    //    b
    //  d e f
    //    h
    ivec2 sp = ivec2(gl_FragCoord.xy);
    vec3 b = Load(sp + ivec2(0, -1));
    vec3 d = Load(sp + ivec2(-1, 0));
    vec3 e = Load(sp);
    vec3 f = Load(sp + ivec2(1, 0));
    vec3 h = Load(sp + ivec2(0, 1));
    // Min and max of ring.
    vec3 mn4 = min(vec3(Min3(b.r, d.r, f.r), Min3(b.g, d.g, f.g), Min3(b.b, d.b, f.b)), h);
    vec3 mx4 = max(vec3(Max3(b.r, d.r, f.r), Max3(b.g, d.g, f.g), Max3(b.b, d.b, f.b)), h);
    // Immediate constants for peak range.
    vec2 peakC = vec2(1.0, -1.0 * 4.0);
    // Limiters, these need to be high precision RCPs. The denominators are
    // kept off zero (an all-black or all-white ring), where FSR divides by
    // zero and leans on the hardware's NaN handling, which SPIR-V leaves
    // undefined; with the guard a 0/0 limit means no sharpening.
    vec3 hitMin = min(mn4, e) * (1.0 / max(4.0 * mx4, vec3(1.0 / 65536.0)));
    vec3 hitMax = (peakC.x - max(mx4, e)) * (1.0 / min(4.0 * mn4 + peakC.y, vec3(-1.0 / 65536.0)));
    vec3 lobeRGB = max(-hitMin, hitMax);
    float lobe = max(float(-FSR_RCAS_LIMIT), min(Max3(lobeRGB.r, lobeRGB.g, lobeRGB.b), 0.0)) * params.x;
    // Resolve. FSR uses its medium precision rcp approximation here, which
    // is 0.3% low at lobe 0 and would darken every pixel at sharpness 0; the
    // exact reciprocal keeps sharpness 0 an identity.
    float rcpL = 1.0 / (4.0 * lobe + 1.0);
    color = vec4((lobe * b + lobe * d + lobe * h + lobe * f + e) * rcpL, 1.0);
}
