#include "../bindless.h"
#include "../shading.h"

// Vertex Shader
struct VSInput
{
	float4 inPos : POSITION;
	float2 inUV : TEXCOORD0;
	float3 inColor : COLOR;
	float3 inNormal : NORMAL;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float4 outPos : UV_POS;
};

cbuffer UBO : register(b0)
{
	matrix model;
	matrix light_matrix[4];
	float4 cascade_splits;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.position = mul(projection, mul(view, mul(model, float4(input.inPos.xyz, 1.0))));
	output.outPos = mul(projection, mul(view, mul(model, float4(input.inPos.xyz, 1.0))));
	return output;
}

// Pixel Shader

#if RAY_TRACED_SHADOWS == 1
	Texture2D shadow_map : register(t5);
#else
	#if LIGHT_TYPE == 0
		TextureCube shadow_map : register(t5);
		SamplerState shadow_map_sampler : register(s5);
	#else
		Texture2DArray shadow_map : register(t5);
		SamplerComparisonState shadow_map_sampler : register(s5);
	#endif
#endif

cbuffer UBOTextures : register(b1)
{
	uint albedoTexId;
	uint normalTexId;
	uint depthTexId;
	uint shadingTexId;
	uint shadowMapTexId;
};

cbuffer PushConstants : register(b2)
{
	float4 light_pos;
	float4 light_color;
	float light_intensity;
	float light_range_square;
	float z_far;
};

struct PSOutput
{
	float3 outDiffuse : SV_Target0;
	float3 outSpecular : SV_Target1;
};

static const float3 sampling_offsets[20] = {
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

#define SHADOW_MAP_CASCADE_COUNT 4
#if RAY_TRACED_SHADOWS == 0
	#if LIGHT_TYPE == 0
		float get_shadow_point(float3 frag_pos, float bias)
		{
			float3 fragToLight = frag_pos - light_pos.xyz;
			float current_depth = length(fragToLight) / z_far;

			float shadow = 0.0;
			
			float view_distance = length(frag_pos - camera_position.xyz);
			int samples = 20;
			float sampling_radius = 0.003;
			for (int i = 0; i < samples; i++)
			{
				float closest_depth = shadow_map.Sample(shadow_map_sampler, fragToLight + sampling_offsets[i] * sampling_radius).r;
				shadow += current_depth - bias < closest_depth ? 1.0 : 0.0;
			}

			shadow /= float(samples);

			return clamp(shadow, 0.0, 1.0);
		}
	#elif LIGHT_TYPE == 1
		#define ROTATE_POISSON
		float get_shadow_dir(float3 frag_pos, float bias, float noise)
		{
			// Select layer based on depth
			float depth = mul(view, float4(frag_pos, 1.0)).z;
			int layer = 0;
			for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; i++)
			{
				if (depth > cascade_splits[i])
					layer = i + 1;
			}

			float4 frag_pos_light_space = mul(light_matrix[layer], float4(frag_pos, 1.0));
			float3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

			float current_depth = proj_coords.z;
			if (current_depth > 1.0)
				return 1.0;

			const float biasModifier = 0.5f;
			if (layer == SHADOW_MAP_CASCADE_COUNT - 1)
			{
				bias *= 1.0 / (200.0 * biasModifier);
			}
			else
			{
				bias *= 1.0 / (cascade_splits[layer] * biasModifier);
			}
			//bias = 0.0005;

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
			// PCF using 12 possion disk taps
			for (int i = 0; i < 12; i++)
			{
				#ifdef ROTATE_POISSON
					float2 offset = mul(POISSON_SAMPLES[i], rotation);
				#else
					float2 offset = POISSON_SAMPLES[i] * filter_size;
				#endif
				float3 sample_coord = float3((proj_coords.xy * float2(0.5, -0.5) + 0.5) + offset, layer);
				shadow += shadow_map.SampleCmpLevelZero(shadow_map_sampler, sample_coord, current_depth - bias);
			}
			shadow /= 12.0f;
			
			//shadow = 0.0f;
			//float closest_depth = shadow_map.Sample(shadow_map_sampler, float3(proj_coords.xy * float2(0.5, -0.5) + 0.5, layer)).r;
			//shadow += current_depth - bias < closest_depth ? 1.0 : 0.0;
			//return closest_depth;
			return clamp(shadow, 0.0, 1.0);
		}
	#endif
#endif

float compute_reflectance(float reflectance)
{
	return 0.16 * reflectance * reflectance;
}

float get_smooth_distance_att(float sqr_distance, float inv_sqr_att_radius)
{
	float factor = sqr_distance * (1.0f / inv_sqr_att_radius);
	float smooth_factor = saturate(1.0 - factor * factor);
	return smooth_factor * smooth_factor;
}

float get_attenuation(float3 pos)
{
	float3 delta = light_pos.xyz - pos;
	float sqr_distance = dot(delta, delta);
	float attenuation = 1.0 / max(sqr_distance, 0.0001);
	attenuation *= get_smooth_distance_att(sqr_distance, light_range_square);
	return attenuation;
}

// Shadow functions and position reconstruction would need additional implementation
// based on your specific depth buffer format and projection setup

PSOutput PSMain(VSOutput input)
{
	float2 inUV = input.outPos.xy / input.outPos.w * float2(0.5, -0.5) + 0.5;
	float depth = SampleTexture(depthTexId, inUV).r;

	float4 shading = SampleTexture(shadingTexId, inUV);
	float metalness = shading.r;
	float roughness = saturate(shading.g);
	roughness *= roughness;
	float specular = shading.b;

	float3 albedo = SampleTexture(albedoTexId, inUV).rgb;
	float3 diffuse_color = albedo * (1.0f - metalness);
	float reflectance = compute_reflectance(specular);

	float3 F0 = (albedo * metalness) + (reflectance * (1.0f - metalness));
	float F90 = 1.0f;

	float3 world_pos = GetWSPosition(inUV, depth);
	float3 P = world_pos.xyz;
	float3 N = normalize(SampleTexture(normalTexId, inUV).rgb * 2.0f - 1.0f);
	float3 V = normalize(camera_position.xyz - P);
	float3 L;
	float light_attenuation = 1.0;
	float shadow = 1.0f;

	#if RAY_TRACED_SHADOWS == 1
		L = normalize(light_pos.xyz);
		#if USE_SHADOWS == 1
			shadow = shadow_map.Sample(textureSampler, inUV).r;
		#endif
	#else
		#if LIGHT_TYPE == 0
			L = normalize(light_pos.xyz - P);
			light_attenuation = get_attenuation(P);
			#if USE_SHADOWS == 1
				//float bias = max(0.005, 0.05 * (1.0 - dot(N, L)));
				float bias = 0.005;
				shadow = get_shadow_point(P, bias);
			#endif
		#elif LIGHT_TYPE == 1
			L = normalize(light_pos.xyz);
			#if USE_SHADOWS == 1
				float constant_bias = 0.0005;
				float slope_bias = 0.001;
				float bias = constant_bias + slope_bias * (1.0 - dot(N, L));
				//float noise = hash13(float3(inUV, frame));
				float offset_scale = 0.05;
				float noise = random(inUV);
				shadow = get_shadow_dir(P + N * offset_scale, bias, noise);
			#endif
		#endif
	#endif
	float3 H = normalize(V + L);

	float NdotL = saturate(dot(N, L));
	float NdotV = saturate(abs(dot(N, V)));
	float NdotH = saturate(dot(N, H));
	float LdotH = saturate(dot(L, H));

	float F_diffuse = Fr_DisneyDiffuse(NdotV, NdotL, LdotH, roughness);
	float3 diffuse = diffuse_color * F_diffuse;

	float3 F = F_Schlick(F0, F90, LdotH);
	float D = D_GGX(NdotH, roughness * roughness);
	float Viz = V_SmithGGXCorrelated(NdotV, NdotL, roughness); 
	float3 F_specular = D * F * Viz;

	PSOutput output;
	output.outDiffuse = shadow * NdotL * (float3(1.0f, 1.0f, 1.0f) - F) * diffuse * light_attenuation * light_intensity * light_color.rgb;
	output.outSpecular = shadow * NdotL * F_specular * light_attenuation * light_intensity * light_color.rgb;
	return output;
}