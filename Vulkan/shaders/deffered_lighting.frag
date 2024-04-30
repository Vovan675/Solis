#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint positionTexId;
	uint albedoTexId;
	uint normalTexId;
	uint depthTexId;
	uint shadingTexId;
} ubo;

layout(push_constant) uniform constants
{
	vec4 camPos;
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

void main()
{
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

	vec3 LIGHT_POS = vec3(2, 2, 0);
	vec3 P = texture(textures[ubo.positionTexId], inUV).rgb;
	vec3 V = normalize(PushConstants.camPos.xyz - P);
	vec3 L = normalize(LIGHT_POS - P);
	L = normalize(vec3(2, 2, 0));
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

	outDiffuse = NdotL * (vec3(1.0f) - F) * diffuse;
	outSpecular = NdotL * F_specular;
}