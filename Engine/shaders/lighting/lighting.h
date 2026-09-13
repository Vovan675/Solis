#pragma once
#include "../common.h"
#include "../shading.h"

#define POINT_SHADOW_Z_NEAR 0.01
#define SHADOW_MAP_CASCADE_COUNT 4

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 1
#define INVALID_LIGHT_INDEX 0xFFFFFFFF

struct Light
{
	float4 position;
	float4 direction;
	float4 radiance;
	float4x4 cascade_view_projection[SHADOW_MAP_CASCADE_COUNT];
	float4 cascade_splits;
	uint type;
	float attenuation_radius;
	uint shadow_map_tex_id;
	uint pad;
};

Light getLight(uint index)
{
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[lights_buffer_id];
	return lights[index];
}

int getCascadeIndex(Light light, float depth)
{
	int layer = 0;
	for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; i++)
	{
		if (depth > light.cascade_splits[i])
			layer = i + 1;
	}
	return layer;
}

float getSmoothDistanceAttenuation(float sqr_distance, float attenuation_radius_sqr)
{
	float factor = sqr_distance / attenuation_radius_sqr;
	float smooth_factor = saturate(1.0 - factor * factor);
	return smooth_factor * smooth_factor;
}

float getLightAttenuation(Light light, float3 pos, out float3 L)
{
	L = normalize(light.direction.xyz);
	if (light.type == LIGHT_TYPE_DIRECTIONAL)
		return 1.0;

	float3 delta = light.position.xyz - pos;
	float sqr_distance = dot(delta, delta);
	L = delta * rsqrt(sqr_distance);
	float attenuation = 1.0 / max(sqr_distance, 0.0001);
	return attenuation * getSmoothDistanceAttenuation(sqr_distance, light.attenuation_radius * light.attenuation_radius);
}

static const float3 SAMPLING_OFFSETS[20] = {
	float3(1, 1, 1), float3(1, -1, 1), float3(-1, -1, 1), float3(-1, 1, 1),
	float3(1, 1, -1), float3(1, -1, -1), float3(-1, -1, -1), float3(-1, 1, -1),
	float3(1, 1, 0), float3(1, -1, 0), float3(-1, -1, 0), float3(-1, 1, 0),
	float3(1, 0, 1), float3(-1, 0, 1), float3(1, 0, -1), float3(-1, 0, -1),
	float3(0, 1, 1), float3(0, -1, 1), float3(0, -1, -1), float3(0, 1, -1)
};

static const float2 POISSON_SAMPLES[12] =
{
	float2( 0.8080623514447717f, 0.35634966406596885f ),
	float2( -0.36194484652519326f, -0.5985631884231994f ),
	float2( -0.2982836364211632f, 0.8305744093470052f ),
	float2( 0.5337220238236804f, -0.6041849049099166f ),
	float2( -0.5059233131611521f, 0.09066539225695357f ),
	float2( 0.23037418644628468f, 0.3009621324938023f ),
	float2( 0.39755147240492156f, 0.8856712796190299f ),
	float2( 0.06742373067366426f, -0.26982848314468244f ),
	float2( 0.791063394340841f, -0.12547253470064718f ),
	float2( -0.8081775625387934f, 0.4752307851711048f ),
	float2( 0.027662833091513947f, -0.8783105422420106f ),
	float2( -0.8595296839803187f, -0.3859107698213548f ),
};


float getCubeViewDepth(float3 frag_to_light)
{
	// Max of sampling coordinates for cubemap is the cube face axis, so the value itself is the view depth
	return max(abs(frag_to_light.x), max(abs(frag_to_light.y), abs(frag_to_light.z)));
}

float getPointShadow(Light light, float3 position, float3 N)
{
	TextureCube shadow_map = ResourceDescriptorHeap[light.shadow_map_tex_id];
	uint shadow_map_width, shadow_map_height;
	shadow_map.GetDimensions(shadow_map_width, shadow_map_height);

	float3 L = normalize(light.position.xyz - position);
	float texel_size = 2.0 * getCubeViewDepth(position - light.position.xyz) / shadow_map_width;
	float normal_bias = 2.0 * texel_size * (1.0 - saturate(dot(N, L)));
	float constant_bias = 3.0 * texel_size;

	float3 frag_to_light = position + N * normal_bias - light.position.xyz;
	float view_depth = getCubeViewDepth(frag_to_light) - constant_bias;

	float z_near = POINT_SHADOW_Z_NEAR;
	float z_far = light.attenuation_radius;
	float current_depth = (z_near / (z_far - z_near)) * (z_far / view_depth - 1.0);

	float shadow = 0.0;
	int samples = 20;
	float sampling_radius = 1.5 * texel_size;
	for (int i = 0; i < samples; i++)
	{
		float closest_depth = shadow_map.SampleLevel(point_wrap_sampler, frag_to_light + SAMPLING_OFFSETS[i] * sampling_radius, 0).r;
		shadow += current_depth > closest_depth ? 1.0 : 0.0;
	}
	shadow /= float(samples);

	return saturate(shadow);
}

#define ROTATE_POISSON
#define SOFT_SHADOWS
float getDirectionalShadow(Light light, float3 frag_pos, float bias, float noise)
{
	Texture2DArray shadow_map = ResourceDescriptorHeap[light.shadow_map_tex_id];
	
	// Select layer based on depth
	float depth = -mul(view, float4(frag_pos, 1.0)).z;
	if (depth > light.cascade_splits[SHADOW_MAP_CASCADE_COUNT - 1])
		return 1.0;
	int layer = getCascadeIndex(light, depth);

	float4 frag_pos_light_space = mul(light.cascade_view_projection[layer], float4(frag_pos, 1.0));
	float3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

	float current_depth = proj_coords.z;
	if (current_depth > 1.0)
		return 1.0;
	const float biasModifier = 0.5f;
	if (layer == SHADOW_MAP_CASCADE_COUNT - 1)
	{
		bias *= 1.0 / (200.0 * biasModifier);
	} else
	{
		bias *= 1.0 / (light.cascade_splits[layer] * biasModifier);
	}

	uint tex_levels, tex_width, tex_height;
	shadow_map.GetDimensions(tex_width, tex_height, tex_levels);
	float scale = 1.0f / (1.0f + layer);
	float texel_size = 1.0f / tex_width;
	float filter_size = texel_size * scale * 4;


	#ifdef ROTATE_POISSON
		float angleSin, angleCos;
		sincos(noise * PI2, angleSin, angleCos);
		float2x2 rotation = get2DRotationMatrix(angleSin, angleCos);
		rotation *= filter_size;
	#endif


	float shadow = 0.0;

	#ifdef SOFT_SHADOWS
		// PCF using 12 possion disk taps
		for (int i = 0; i < 12; i++)
		{
			#ifdef ROTATE_POISSON
				float2 offset = mul(POISSON_SAMPLES[i], rotation);
			#else
				float2 offset = POISSON_SAMPLES[i] * filter_size;
			#endif
			float3 sample_coord = float3((proj_coords.xy * float2(0.5, -0.5) + 0.5) + offset, layer);
			shadow += shadow_map.SampleCmpLevelZero(shadow_clamp_sampler, sample_coord, current_depth - bias);
		}
		shadow /= 12.0f;
	#else
		float3 sample_coord = float3((proj_coords.xy * float2(0.5, -0.5) + 0.5), layer);
		shadow = shadow_map.SampleCmpLevelZero(shadow_clamp_sampler, sample_coord, current_depth - bias);
	#endif

	return saturate(shadow);
}

float getLightShadow(Light light, float3 position, float3 N, float noise)
{
	if (light.shadow_map_tex_id == 0)
		return 1.0;

	if (light.type == LIGHT_TYPE_DIRECTIONAL)
	{
		float3 L = normalize(light.direction.xyz);
		float constant_bias = 0.0005;
		float slope_bias = 0.001;
		float bias = constant_bias + slope_bias * (1.0 - dot(N, L));
		float offset_scale = 0.05;
		return getDirectionalShadow(light, position + N * offset_scale, bias, noise);
	}

	return getPointShadow(light, position, N);
}

void calculateDirectLight(Light light, float3 P, float3 N, float3 V, float3 albedo, float metalness, float perceptual_roughness, float specular, float visibility, out float3 out_diffuse, out float3 out_specular)
{
	float3 L;
	float light_attenuation = getLightAttenuation(light, P, L);
	float3 H = normalize(V + L);
	float NdotL = saturate(dot(N, L));
	float NdotV = saturate(abs(dot(N, V)));
	float NdotH = saturate(dot(N, H));
	float LdotH = saturate(dot(L, H));

	float alpha = perceptual_roughness * perceptual_roughness;
	float3 diffuse_color = albedo * (1.0 - metalness);
	float3 F0 = computeF0(albedo, metalness, specular);
	float F90 = 1.0;

	float F_diffuse = Fr_DisneyDiffuse(NdotV, NdotL, LdotH, perceptual_roughness);
	float3 diffuse = diffuse_color * F_diffuse / PI;

	float3 F = FresnelSchlick(F0, F90, LdotH);
	float D = D_GGX(NdotH, alpha * alpha);
	float Viz = V_SmithGGXCorrelated(NdotV, NdotL, alpha);
	float3 F_specular = D * F * Viz;

	float3 irradiance = light.radiance.rgb * light_attenuation * NdotL * visibility;
	out_diffuse = diffuse * irradiance;
	out_specular = F_specular * irradiance;
}