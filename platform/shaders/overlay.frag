#version 450

// The scalable text overlay (platform/text_overlay.h): an output-sized,
// premultiplied image fetched one texel per output pixel; the pipeline
// blends it over the regions already drawn (one, one minus source alpha).
layout(set = 2, binding = 0) uniform sampler2D overlayTexture;

layout(location = 0) out vec4 color;

void main()
{
    color = texelFetch(overlayTexture, ivec2(floor(gl_FragCoord.xy)), 0);
}
