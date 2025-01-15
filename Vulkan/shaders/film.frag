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

vec3 uncharted_2(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
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
    //value = vec4(uncharted_2(value.rgb), 1.0);
    value = LinearToSRGB(value);
    outColor = vec4(value.rgb, 1.0);
    //outColor = vec4(1, 0, 0, 1.0);
}