#include "common.h"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint present_mode;
	uint composite_final_tex_id;
	uint albedo_tex_id;
	uint shading_tex_id;
	uint normal_tex_id;
	uint depth_tex_id;
	uint light_diffuse_id;
	uint light_specular_id;
	uint brdf_lut_id;
	uint ssao_id;
} ubo;

void main() {
    uint mode = ubo.present_mode;

    vec2 uv = inUV;
    vec4 value = vec4(1, 1, 1, 1);
    const float border = 0.001;

    if (mode == 0)
    {
        if (uv.x < 1.0 / 3.0 - border)
        {
            mode = 1;
        } else if (uv.x > 1.0 / 3.0 && uv.x < 2.0 / 3.0)
        {
            mode = 2;
        } else
        {
            mode = 3;
        }
        uv.x *= 3.0;
    }
    
    vec4 composite_final = texture(textures[ubo.composite_final_tex_id], uv);
    vec4 albedo = texture(textures[ubo.albedo_tex_id], uv);
    vec4 shading = texture(textures[ubo.shading_tex_id], uv);
    vec4 normal = texture(textures[ubo.normal_tex_id], uv);
    float depth = texture(textures[ubo.depth_tex_id], uv).r;
    vec3 diffuse = texture(textures[ubo.light_diffuse_id], uv).xyz;
    vec3 specular = texture(textures[ubo.light_specular_id], uv).xyz;
    vec2 brdf_lut = texture(textures[ubo.brdf_lut_id], uv).xy;
    float ssao = texture(textures[ubo.ssao_id], uv).r;

    if (mode == 1)
    {
        value = composite_final;
    } else if (mode == 2)
    {
        value = albedo;
    } else if (mode == 3)
    {
        value = vec4(shading.r, shading.r, shading.r, 1.0);
    } else if (mode == 4)
    {
        value = vec4(shading.g, shading.g, shading.g, 1.0);
    } else if (mode == 5)
    {
        value = vec4(shading.b, shading.b, shading.b, 1.0);
    } else if (mode == 6)
    {   
        value = normal;
    } else if (mode == 7)
    {   
        value = vec4(depth, depth, depth, 1.0);
    } else if (mode == 8)
    {   
        vec3 view_pos = getVSPosition(uv, depth);
	    vec4 world_pos = inverse(view) * vec4(view_pos, 1.0);
        value = vec4(world_pos.xyz, 1.0);
    } else if (mode == 9)
    {   
        value = vec4(diffuse, 1.0);
    } else if (mode == 10)
    {   
        value = vec4(specular, 1.0);
    } else if (mode == 11)
    {   
        value = vec4(brdf_lut, 0.0, 1.0);
    } else if (mode == 12)
    {   
        value = vec4(ssao, ssao, ssao, 1.0);
    }
    outColor = vec4(LinearToSRGB(value).rgb, 1.0);
}