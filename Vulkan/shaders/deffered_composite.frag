#include "common.h"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
    uint lighting_diffuse_tex_id;
	uint lighting_specular_tex_id;
	uint albedo_tex_id;
	uint normal_tex_id;
	uint depth_tex_id;
	uint shading_tex_id;
	uint brdf_lut_tex_id;
	uint ssao_tex_id;
} ubo;

layout (set=0, binding=1) uniform samplerCube irradiance_tex;
layout (set=0, binding=2) uniform samplerCube prefilter_tex;

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
    vec3 normal = normalize(texture(textures[ubo.normal_tex_id], inUV).rgb);

    vec3 light_diffuse = texture(textures[ubo.lighting_diffuse_tex_id], inUV).rgb;
    vec3 light_specular = texture(textures[ubo.lighting_specular_tex_id], inUV).rgb;

    vec4 shading = texture(textures[ubo.shading_tex_id], inUV).rgba;
	float metalness = shading.r;
	float roughness = clamp(shading.g, 0.045f, 1.0f);

    // IBL
    vec3 f0 = mix(vec3(0.04), albedo.rgb, metalness);
    vec3 f = F_Schlick(f0, 1.0, roughness);
    vec3 kd = (1.0 - f);

    vec3 irradiance = SRGBtoLinear(texture(irradiance_tex, normal)).rgb;
    vec3 ibl_diffuse = irradiance * albedo.rgb * kd;

    vec3 view_pos = getVSPosition(inUV, depth);
	vec4 world_pos = inverse(view) * vec4(view_pos, 1.0);

    vec3 v = normalize(camera_position.xyz - world_pos.rgb);
	float NdotV = clamp(abs(dot(normal, v)), 0.0f, 1.0f);
    vec3 brdf_lut = texture(textures[ubo.brdf_lut_tex_id], vec2(NdotV, 1.0 - roughness)).rgb;

    vec3 reflection = -normalize(reflect(v, normal));
    float lod = roughness * textureQueryLevels(prefilter_tex); // num mip maps

    vec3 prefilter = SRGBtoLinear(textureLod(prefilter_tex, reflection, lod)).rgb;

    float ssao = texture(textures[ubo.ssao_tex_id], inUV).r;

    vec3 diffuse = ibl_diffuse * ssao;
    vec3 specular = prefilter * (f0 * brdf_lut.x + brdf_lut.y);
    vec3 ibl = diffuse + specular;
    outColor = vec4(albedo.rgb * light_diffuse + light_specular + ibl * 0.5, 1);
    //outColor.rgb = LinearToSRGB(outColor).rgb;
}