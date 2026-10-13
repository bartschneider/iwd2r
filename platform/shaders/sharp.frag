#version 450

// Sharp bilinear: nearest at texel centres, linear only within one output
// pixel of each texel edge, so a non-integer scale keeps pixels crisp
// without uneven widths. With p in texel units centred on integers,
// uv = floor(p) + clamp((fract(p) - 0.5) * scale, -0.5, 0.5) + 0.5, scale the
// output pixels per texel; the linear sampler reads uv + 0.5 (back to
// texel-edge space). At integer scales every output pixel centre lands on a
// texel centre, so the result equals nearest.
layout(set = 2, binding = 0) uniform sampler2D frameTexture;

layout(set = 3, binding = 0) uniform Region {
    vec4 frameRect; // left, top, right, bottom in texels
    vec4 dest; // destination origin x, y in output pixels; scale x, y (output pixels per texel)
};

layout(location = 0) out vec4 color;

void main()
{
    vec2 q = frameRect.xy + (gl_FragCoord.xy - dest.xy) / dest.zw;
    vec2 p = q - 0.5;
    vec2 uv = floor(p) + clamp((fract(p) - 0.5) * dest.zw, -0.5, 0.5) + 0.5;
    // Stay inside the region's frame rectangle: no bleed from its neighbours.
    vec2 s = clamp(uv, frameRect.xy, frameRect.zw - 1.0) + 0.5;
    color = vec4(texture(frameTexture, s / vec2(textureSize(frameTexture, 0))).rgb, 1.0);
}
