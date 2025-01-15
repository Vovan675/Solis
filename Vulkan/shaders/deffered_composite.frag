#include "common.h"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
    uint lighting_diffuse_tex_id;
	uint lighting_specular_tex_id;
    uint indirect_ambient_tex_id;
	uint indirect_specular_tex_id;
	uint albedo_tex_id;
	uint depth_tex_id;
} ubo;

// F - Fresnel Schlick
vec3 F_Schlick(in vec3 f0, in float f90, in float u)
{
	return f0 + (f90 - f0) * pow(1.0f - u, 5.0f);
}

void main() {
    float depth = texture(textures[ubo.depth_tex_id], inUV).r;

    if (depth == 1.0)
        discard;

    vec4 albedo = texture(textures[ubo.albedo_tex_id], inUV);

    vec3 light_diffuse = texture(textures[ubo.lighting_diffuse_tex_id], inUV).rgb;
    vec3 light_specular = texture(textures[ubo.lighting_specular_tex_id], inUV).rgb;

    vec3 indirect_ambient = texture(textures[ubo.indirect_ambient_tex_id], inUV).rgb;
    vec3 indirect_specular = texture(textures[ubo.indirect_specular_tex_id], inUV).rgb;

    outColor = vec4(albedo.rgb * light_diffuse + light_specular + indirect_ambient + indirect_specular, 1);
}