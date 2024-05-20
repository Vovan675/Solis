#include "common.h"

layout(location = 0) in vec4 inPos;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=1) uniform UBO
{
	uint positionTexId;
	uint albedoTexId;
	uint normalTexId;
	uint depthTexId;
	uint shadingTexId;
} ubo;

layout(push_constant) uniform constants
{
	vec4 light_pos;
	vec4 light_color;
	float light_intensity;
	float light_range_square; // radius ^ 2
	float padding[2];
} PushConstants;

layout(location = 0) out vec3 outDiffuse;
layout(location = 1) out vec3 outSpecular;

const float PI = 3.14159265359;

// F - Fresnel Schlick
vec3 F_Schlick(in vec3 f0, in float f90, in float u)
{
	return f0 + (f90 - f0) * pow(1.0f - u, 5.0f);
}

// D
float D_GGX(float NdotH, float m)
{
	float m2 = m * m;
	float f = (NdotH * m2 - NdotH) * NdotH + 1.0f;
	return m2 / (PI * f * f);
}

// V
float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness)
{
	float alphaRoughnessSq = roughness * roughness;

	float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
	float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

	float GGX = GGXV + GGXL;
	if (GGX > 0.0)
	{
		return 0.5 / GGX;
	}
	return 0.0;
}


// Diffuse
float Fr_DisneyDiffuse(float NdotV, float NdotL, float LdotH, float linearRoughness)
{
	float energyBias = mix(0, 0.5, linearRoughness);
	float energyFactor = mix(1.0, 1.0 / 1.51, linearRoughness);
	float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness;
	vec3 f0 = vec3(1.0f, 1.0f, 1.0f);
	float lightScatter = F_Schlick(f0, fd90, NdotL).r;
	float viewScatter = F_Schlick(f0, fd90, NdotV).r;
	return lightScatter * viewScatter * energyFactor;
}

float compute_reflectance(float reflectance)
{
	// Compute reflectance for dielectric (0.04 for reflectance = 0.5)
	return 0.16 * reflectance * reflectance;
}

// Moving Frostbite to pbr
float get_smooth_distance_att(float sqr_distance, float inv_sqr_att_radius)
{
	float factor = sqr_distance * (1 / inv_sqr_att_radius);
	float smooth_factor = clamp(1.0 - factor * factor, 0.0, 1.0);
	return smooth_factor * smooth_factor;
}

float get_attenuation(vec3 pos)
{
	vec3 delta = PushConstants.light_pos.xyz - pos;
	float sqr_distance = dot(delta, delta);
	float attenuation = 1.0 / max(sqr_distance, 0.01 * 0.01);
	attenuation *= get_smooth_distance_att(sqr_distance, PushConstants.light_range_square);
	return attenuation;
}

vec3 getVSPosition(vec2 uv, float hardware_depth)
{
    vec4 pos = inverse(projection) * vec4(uv * 2 - 1, hardware_depth, 1.0f);
    pos /= pos.w;
    return pos.xyz;
}

void main()
{
	vec2 inUV = inPos.xy / inPos.w * 0.5 + 0.5;
	float depth = texture(textures[ubo.depthTexId], inUV).r;

	vec4 shading = texture(textures[ubo.shadingTexId], inUV).rgba;
	float metalness = shading.r;
	float roughness = clamp(shading.g, 0.045f, 1.0f);
	roughness *= roughness;
	float specular = shading.b;

	vec3 albedo = texture(textures[ubo.albedoTexId], inUV).rgb;
	// Metals only reflect light, while dielectrics has diffuse light
	vec3 diffuse_color = albedo * (1.0f - metalness);
	float reflectance = compute_reflectance(specular);

	// As I said before, albedo as reflection for metallic, and reflectance based for non-metals
	vec3 F0 = (albedo * metalness) + (reflectance * (1.0f - metalness));
	float F90 = 1.0f;


	vec3 view_pos = getVSPosition(inUV, depth);
	vec4 world_pos = inverse(view) * vec4(view_pos, 1.0);
	world_pos.xyz /= world_pos.w;
	//vec3 P = texture(textures[ubo.positionTexId], inUV).rgb;
	vec3 P = world_pos.xyz;
	vec3 V = normalize(camera_position.xyz - P);
	vec3 L = normalize(PushConstants.light_pos.xyz - P);
	//L = normalize(vec3(2, 2, 0));
	vec3 N = normalize(texture(textures[ubo.normalTexId], inUV).rgb);
	vec3 H = normalize(V + L);

	float NdotL = clamp(dot(N, L), 0.001f, 1.0f);
	float NdotV = clamp(abs(dot(N, V)), 0.001f, 1.0f);
	float NdotH = clamp(dot(N, H), 0.0f, 1.0f);
	float LdotH = clamp(dot(L, H), 0.0f, 1.0f);

	// Diffuse BRDF
	float F_diffuse = Fr_DisneyDiffuse(NdotV, NdotL, LdotH, roughness);
	vec3 diffuse = diffuse_color * F_diffuse;

	// Specular BRDF
	vec3 F = F_Schlick(F0, F90, LdotH);
	float D = D_GGX(NdotH, roughness);
	float Viz = V_SmithGGXCorrelated(NdotV, NdotL, roughness); 
	vec3 F_specular = D * F * Viz;

	float attenuation = get_attenuation(P);
	outDiffuse = NdotL * (vec3(1.0f) - F) * diffuse * attenuation * PushConstants.light_intensity * PushConstants.light_color.rgb;
	outSpecular = NdotL * F_specular * attenuation * PushConstants.light_intensity * PushConstants.light_color.rgb;
}