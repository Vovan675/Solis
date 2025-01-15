#include "common.h"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
    uint lighting_diffuse_tex_id;
	uint lighting_specular_tex_id;
	uint albedo_tex_id;
} ubo;


void main() {
    vec4 albedo = texture(textures[ubo.albedo_tex_id], inUV);

    vec3 light_diffuse = texture(textures[ubo.lighting_diffuse_tex_id], inUV).rgb;
    vec3 light_specular = texture(textures[ubo.lighting_specular_tex_id], inUV).rgb;

    outColor = vec4(albedo.rgb * light_diffuse + light_specular, 1);
}