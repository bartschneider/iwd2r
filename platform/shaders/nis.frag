#version 450

// NVIDIA Image Scaling (NVScaler) from NIS_Scaler.h and NIS_Config.h, SDK
// v1.0.3: the 6-tap scaler with four directional filters and adaptive
// sharpening, SDR, 32-bit, no viewport or NV12 support.
// Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. MIT licence; see
// docs/THIRD_PARTY.md.
//
// Ported from the SDK's compute shader to a fragment shader: each output
// pixel reads its own 6x6 luma support and derives the four edge-map
// entries it interpolates, where NVScaler shares both through groupshared
// tiles. The arithmetic per pixel is NVScaler's. Luma taps are texelFetch
// reads clamped to the region's frame rectangle; the chroma tap is the
// SDK's bilinear sample, kept half a texel inside it. The coefficient
// tables come in a uniform block, and NVScalerUpdateConfig's constants are
// computed from the sharpness.
layout(set = 2, binding = 0) uniform sampler2D frameTexture;

layout(set = 3, binding = 0) uniform Region {
    vec4 frameRect; // left, top, right, bottom in texels
    vec4 dest; // destination origin x, y in output pixels; scale x, y (output pixels per texel)
    vec4 params; // x: sharpness 0..1, NVScalerUpdateConfig's sharpness
};

layout(location = 0) out vec4 color;

#define kPhaseCount 64

// coef_scale and coef_usm from NIS_Config.h (the presenter pushes them
// once per draw): phase k's six taps are table[2k].xyzw, table[2k + 1].xy.
layout(set = 3, binding = 1) uniform Coefficients {
    vec4 coefScale[2 * kPhaseCount];
    vec4 coefUsm[2 * kPhaseCount];
};

float CoefScale(int phase, int i) { return coefScale[phase * 2 + (i >> 2)][i & 3]; }
float CoefUsm(int phase, int i) { return coefUsm[phase * 2 + (i >> 2)][i & 3]; }

// NVScalerUpdateConfig's constants (SDR).
float kDetectRatio;
float kDetectThres;
float kMinContrastRatio;
float kRatioNorm;
float kContrastBoost;
float kEps;
float kSharpStartY;
float kSharpScaleY;
float kSharpStrengthMin;
float kSharpStrengthScale;
float kSharpLimitMin;
float kSharpLimitScale;

void UpdateConfig(float sharpness)
{
    sharpness = clamp(sharpness, 0.0, 1.0);
    float sharpen_slider = sharpness - 0.5; // Map 0 to 1 to -0.5 to +0.5
    // Different range for 0 to 50% vs 50% to 100%: sharpness 0% maps to no
    // sharpening, 100% does not over-sharpen.
    float MaxScale = (sharpen_slider >= 0.0) ? 1.25 : 1.75;
    float MinScale = (sharpen_slider >= 0.0) ? 1.25 : 1.0;
    float LimitScale = (sharpen_slider >= 0.0) ? 1.25 : 1.0;
    kDetectRatio = 2.0 * 1127.0 / 1024.0;
    kDetectThres = 64.0 / 1024.0;
    kMinContrastRatio = 2.0;
    float kMaxContrastRatio = 10.0;
    kSharpStartY = 0.45;
    float kSharpEndY = 0.9;
    kSharpStrengthMin = max(0.0, 0.4 + sharpen_slider * MinScale * 1.2);
    float kSharpStrengthMax = 1.6 + sharpen_slider * MaxScale * 1.8;
    kSharpLimitMin = max(0.1, 0.14 + sharpen_slider * LimitScale * 0.32);
    float kSharpLimitMax = 0.5 + sharpen_slider * LimitScale * 0.6;
    kRatioNorm = 1.0 / (kMaxContrastRatio - kMinContrastRatio);
    kSharpScaleY = 1.0 / (kSharpEndY - kSharpStartY);
    kSharpStrengthScale = kSharpStrengthMax - kSharpStrengthMin;
    kSharpLimitScale = kSharpLimitMax - kSharpLimitMin;
    kContrastBoost = 1.0;
    kEps = 1.0 / 255.0;
}

float getY(vec3 rgba)
{
    return 0.2126 * rgba.x + 0.7152 * rgba.y + 0.0722 * rgba.z;
}

// The edge map of the 3x3 luma block whose top-left is p[i][j].
vec4 GetEdgeMap(float p[6][6], int i, int j)
{
    float g_0 = abs(p[0 + i][0 + j] + p[0 + i][1 + j] + p[0 + i][2 + j] - p[2 + i][0 + j] - p[2 + i][1 + j] - p[2 + i][2 + j]);
    float g_45 = abs(p[1 + i][0 + j] + p[0 + i][0 + j] + p[0 + i][1 + j] - p[2 + i][1 + j] - p[2 + i][2 + j] - p[1 + i][2 + j]);
    float g_90 = abs(p[0 + i][0 + j] + p[1 + i][0 + j] + p[2 + i][0 + j] - p[0 + i][2 + j] - p[1 + i][2 + j] - p[2 + i][2 + j]);
    float g_135 = abs(p[1 + i][0 + j] + p[2 + i][0 + j] + p[2 + i][1 + j] - p[0 + i][1 + j] - p[0 + i][2 + j] - p[1 + i][2 + j]);

    float g_0_90_max = max(g_0, g_90);
    float g_0_90_min = min(g_0, g_90);
    float g_45_135_max = max(g_45, g_135);
    float g_45_135_min = min(g_45, g_135);

    float e_0_90 = 0.0;
    float e_45_135 = 0.0;

    if (g_0_90_max + g_45_135_max == 0.0)
        return vec4(0.0, 0.0, 0.0, 0.0);

    e_0_90 = min(g_0_90_max / (g_0_90_max + g_45_135_max), 1.0);
    e_45_135 = 1.0 - e_0_90;

    bool c_0_90 = (g_0_90_max > (g_0_90_min * kDetectRatio)) && (g_0_90_max > kDetectThres) && (g_0_90_max > g_45_135_min);
    bool c_45_135 = (g_45_135_max > (g_45_135_min * kDetectRatio)) && (g_45_135_max > kDetectThres) && (g_45_135_max > g_0_90_min);
    bool c_g_0_90 = g_0_90_max == g_0;
    bool c_g_45_135 = g_45_135_max == g_45;

    float f_e_0_90 = (c_0_90 && c_45_135) ? e_0_90 : 1.0;
    float f_e_45_135 = (c_0_90 && c_45_135) ? e_45_135 : 1.0;

    float weight_0 = (c_0_90 && c_g_0_90) ? f_e_0_90 : 0.0;
    float weight_90 = (c_0_90 && !c_g_0_90) ? f_e_0_90 : 0.0;
    float weight_45 = (c_45_135 && c_g_45_135) ? f_e_45_135 : 0.0;
    float weight_135 = (c_45_135 && !c_g_45_135) ? f_e_45_135 : 0.0;

    return vec4(weight_0, weight_90, weight_45, weight_135);
}

float CalcLTI(float p0, float p1, float p2, float p3, float p4, float p5, int phase_index)
{
    bool selector = (phase_index <= kPhaseCount / 2);
    float sel = selector ? p0 : p3;
    float a_min = min(min(p1, p2), sel);
    float a_max = max(max(p1, p2), sel);
    sel = selector ? p2 : p5;
    float b_min = min(min(p3, p4), sel);
    float b_max = max(max(p3, p4), sel);

    float a_cont = a_max - a_min;
    float b_cont = b_max - b_min;

    float cont_ratio = max(a_cont, b_cont) / (min(a_cont, b_cont) + kEps);
    return (1.0 - clamp((cont_ratio - kMinContrastRatio) * kRatioNorm, 0.0, 1.0)) * kContrastBoost;
}

vec4 GetInterpEdgeMap(vec4 edge[2][2], float phase_frac_x, float phase_frac_y)
{
    vec4 h0 = mix(edge[0][0], edge[0][1], phase_frac_x);
    vec4 h1 = mix(edge[1][0], edge[1][1], phase_frac_x);
    return mix(h0, h1, phase_frac_y);
}

float EvalPoly6(float pxl[6], int phase_int)
{
    float y = 0.0;
    for (int i = 0; i < 6; ++i)
        y += CoefScale(phase_int, i) * pxl[i];
    float y_usm = 0.0;
    for (int i = 0; i < 6; ++i)
        y_usm += CoefUsm(phase_int, i) * pxl[i];

    // let's compute a piece-wise ramp based on luma
    float y_scale = 1.0 - clamp((y - kSharpStartY) * kSharpScaleY, 0.0, 1.0);

    // scale the ramp to sharpen as a function of luma
    float y_sharpness = y_scale * kSharpStrengthScale + kSharpStrengthMin;

    y_usm *= y_sharpness;

    // scale the ramp to limit USM as a function of luma
    float y_sharpness_limit = (y_scale * kSharpLimitScale + kSharpLimitMin) * y;

    y_usm = min(y_sharpness_limit, max(-y_sharpness_limit, y_usm));
    // reduce ringing
    y_usm *= CalcLTI(pxl[0], pxl[1], pxl[2], pxl[3], pxl[4], pxl[5], phase_int);

    return y + y_usm;
}

float FilterNormal(float p[6][6], int phase_x_frac_int, int phase_y_frac_int)
{
    float h_acc = 0.0;
    for (int j = 0; j < 6; ++j) {
        float v_acc = 0.0;
        for (int i = 0; i < 6; ++i)
            v_acc += p[i][j] * CoefScale(phase_y_frac_int, i);
        h_acc += v_acc * CoefScale(phase_x_frac_int, j);
    }
    return h_acc;
}

float AddDirFilters(float p[6][6], float phase_x_frac, float phase_y_frac, int phase_x_frac_int, int phase_y_frac_int, vec4 w)
{
    float f = 0.0;
    if (w.x > 0.0) {
        // 0 deg filter
        float interp0Deg[6];
        for (int i = 0; i < 6; ++i)
            interp0Deg[i] = mix(p[i][2], p[i][3], phase_x_frac);
        f += EvalPoly6(interp0Deg, phase_y_frac_int) * w.x;
    }
    if (w.y > 0.0) {
        // 90 deg filter
        float interp90Deg[6];
        for (int i = 0; i < 6; ++i)
            interp90Deg[i] = mix(p[2][i], p[3][i], phase_y_frac);
        f += EvalPoly6(interp90Deg, phase_x_frac_int) * w.y;
    }
    if (w.z > 0.0) {
        // 45 deg filter
        float pphase_b45 = 0.5 + 0.5 * (phase_x_frac - phase_y_frac);

        float temp_interp45Deg[7];
        temp_interp45Deg[1] = mix(p[2][1], p[1][2], pphase_b45);
        temp_interp45Deg[3] = mix(p[3][2], p[2][3], pphase_b45);
        temp_interp45Deg[5] = mix(p[4][3], p[3][4], pphase_b45);
        {
            pphase_b45 = pphase_b45 - 0.5;
            float a = (pphase_b45 >= 0.0) ? p[0][2] : p[2][0];
            float b = (pphase_b45 >= 0.0) ? p[1][3] : p[3][1];
            float c = (pphase_b45 >= 0.0) ? p[2][4] : p[4][2];
            float d = (pphase_b45 >= 0.0) ? p[3][5] : p[5][3];
            temp_interp45Deg[0] = mix(p[1][1], a, abs(pphase_b45));
            temp_interp45Deg[2] = mix(p[2][2], b, abs(pphase_b45));
            temp_interp45Deg[4] = mix(p[3][3], c, abs(pphase_b45));
            temp_interp45Deg[6] = mix(p[4][4], d, abs(pphase_b45));
        }

        float interp45Deg[6];
        float pphase_p45 = phase_x_frac + phase_y_frac;
        if (pphase_p45 >= 1.0) {
            for (int i = 0; i < 6; i++)
                interp45Deg[i] = temp_interp45Deg[i + 1];
            pphase_p45 = pphase_p45 - 1.0;
        } else {
            for (int i = 0; i < 6; i++)
                interp45Deg[i] = temp_interp45Deg[i];
        }

        f += EvalPoly6(interp45Deg, int(pphase_p45 * 64.0)) * w.z;
    }
    if (w.w > 0.0) {
        // 135 deg filter
        float pphase_b135 = 0.5 * (phase_x_frac + phase_y_frac);

        float temp_interp135Deg[7];
        temp_interp135Deg[1] = mix(p[3][1], p[4][2], pphase_b135);
        temp_interp135Deg[3] = mix(p[2][2], p[3][3], pphase_b135);
        temp_interp135Deg[5] = mix(p[1][3], p[2][4], pphase_b135);
        {
            pphase_b135 = pphase_b135 - 0.5;
            float a = (pphase_b135 >= 0.0) ? p[5][2] : p[3][0];
            float b = (pphase_b135 >= 0.0) ? p[4][3] : p[2][1];
            float c = (pphase_b135 >= 0.0) ? p[3][4] : p[1][2];
            float d = (pphase_b135 >= 0.0) ? p[2][5] : p[0][3];
            temp_interp135Deg[0] = mix(p[4][1], a, abs(pphase_b135));
            temp_interp135Deg[2] = mix(p[3][2], b, abs(pphase_b135));
            temp_interp135Deg[4] = mix(p[2][3], c, abs(pphase_b135));
            temp_interp135Deg[6] = mix(p[1][4], d, abs(pphase_b135));
        }

        float interp135Deg[6];
        float pphase_p135 = 1.0 + (phase_x_frac - phase_y_frac);
        if (pphase_p135 >= 1.0) {
            for (int i = 0; i < 6; ++i)
                interp135Deg[i] = temp_interp135Deg[i + 1];
            pphase_p135 = pphase_p135 - 1.0;
        } else {
            for (int i = 0; i < 6; ++i)
                interp135Deg[i] = temp_interp135Deg[i];
        }

        f += EvalPoly6(interp135Deg, int(pphase_p135 * 64.0)) * w.w;
    }
    return f;
}

void main()
{
    UpdateConfig(params.x);

    // The output pixel centre in input texel space (texel k's centre at k),
    // NVScaler's srcX/srcY.
    vec2 q = frameRect.xy + (gl_FragCoord.xy - dest.xy) / dest.zw;
    vec2 src = q - 0.5;
    vec2 fsrc = floor(src);
    // fractional part and discretized phase
    float fx = src.x - fsrc.x;
    float fy = src.y - fsrc.y;
    int fx_int = int(fx * float(kPhaseCount));
    int fy_int = int(fy * float(kPhaseCount));

    // The 6x6 luma support: p[i][j] is texel (floor(srcX) - 2 + j,
    // floor(srcY) - 2 + i).
    ivec2 lo = ivec2(frameRect.xy);
    ivec2 hi = ivec2(frameRect.zw) - 1;
    ivec2 base = ivec2(fsrc) - 2;
    float p[6][6];
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            p[i][j] = getY(texelFetch(frameTexture, clamp(base + ivec2(j, i), lo, hi), 0).rgb);

    // generate weights for directional filters: the edge maps centred on
    // the four texels around the sample
    vec4 edge[2][2];
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            edge[i][j] = GetEdgeMap(p, 1 + i, 1 + j);
    vec4 w = GetInterpEdgeMap(edge, fx, fy);

    // weigth for luma
    float baseWeight = 1.0 - w.x - w.y - w.z - w.w;

    // final luma is a weighted product of directional & normal filters
    float opY = 0.0;

    // get traditional scaler filter output
    opY += FilterNormal(p, fx_int, fy_int) * baseWeight;

    // get directional filter bank output
    opY += AddDirFilters(p, fx, fy, fx_int, fy_int, w);

    // do bilinear tap for chroma upscaling
    vec2 s = clamp(q, frameRect.xy + 0.5, frameRect.zw - 0.5);
    vec4 op = texture(frameTexture, s / vec2(textureSize(frameTexture, 0)));
    float y = getY(op.rgb);

    float corr = opY - y;
    color = vec4(op.rgb + corr, 1.0);
}
