#version 450

// Smooth: plain bilinear over the region's frame rectangle.
layout(set = 2, binding = 0) uniform sampler2D frameTexture;

layout(set = 3, binding = 0) uniform Region {
    vec4 frameRect; // left, top, right, bottom in texels
    vec4 dest; // destination origin x, y in output pixels; scale x, y (output pixels per texel)
};

layout(location = 0) out vec4 color;

void main()
{
    vec2 q = frameRect.xy + (gl_FragCoord.xy - dest.xy) / dest.zw;
    vec2 s = clamp(q, frameRect.xy + 0.5, frameRect.zw - 0.5);
    color = vec4(texture(frameTexture, s / vec2(textureSize(frameTexture, 0))).rgb, 1.0);
}
