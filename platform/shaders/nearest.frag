#version 450

// Nearest: the texel under the output pixel's centre, fetched without
// filtering, so integer scales repeat each texel exactly.
layout(set = 2, binding = 0) uniform sampler2D frameTexture;

layout(set = 3, binding = 0) uniform Region {
    vec4 frameRect; // left, top, right, bottom in texels
    vec4 dest; // destination origin x, y in output pixels; scale x, y (output pixels per texel)
};

layout(location = 0) out vec4 color;

void main()
{
    // q: texel space, texel k's centre at k + 0.5.
    vec2 q = frameRect.xy + (gl_FragCoord.xy - dest.xy) / dest.zw;
    ivec2 t = clamp(ivec2(floor(q)), ivec2(frameRect.xy), ivec2(frameRect.zw) - 1);
    color = vec4(texelFetch(frameTexture, t, 0).rgb, 1.0);
}
