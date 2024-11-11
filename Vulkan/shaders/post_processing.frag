#include "common.h"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint composite_final_tex_id;
	float use_vignette;
	float vignette_radius;
	float vignette_smoothness;
} ubo;

float get_vignette(vec2 uv, float radius, float smoothness)
{
    float dist = radius - distance(uv, vec2(0.5, 0.5));
    return smoothstep(-smoothness, smoothness, dist);
}

void main() {
    vec2 uv = inUV;
    float vignette = 1.0;
    if (ubo.use_vignette > 0.5f)
    {
        vignette = get_vignette(uv, ubo.vignette_radius, ubo.vignette_smoothness);
    }

    vec4 value = vec4(1, 1, 1, 1);
    vec4 composite_final = texture(textures[ubo.composite_final_tex_id], uv);
    value = composite_final * vignette;
    outColor = vec4(LinearToSRGB(value).rgb, 1.0);
}