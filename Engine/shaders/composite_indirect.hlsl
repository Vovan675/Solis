#include "bindless.h"
#include "shading.h"
#include "ddgi/ddgi_common.hlsl"

struct VSInput {
	float2 uv : TEXCOORD0;
};

struct PSOutput {
	float4 ambient : SV_Target0;
	float4 specular : SV_Target1;
};

cbuffer UBO : register(b0)
{
	uint irradiance_tex_id;
	uint prefilter_tex_id;
	uint lighting_diffuse_tex_id;
	uint lighting_specular_tex_id;
	uint albedo_tex_id;
	uint normal_tex_id;
	uint depth_tex_id;
	uint shading_tex_id;
	uint brdf_lut_tex_id;
	uint ssao_tex_id;
	uint ssr_tex_id;
};

static TextureCube irradiance_tex = ResourceDescriptorHeap[irradiance_tex_id];
static TextureCube prefilter_tex = ResourceDescriptorHeap[prefilter_tex_id];

PSOutput PSMain(VSInput input)
{
	PSOutput output;

	float depth = SampleTexture(depth_tex_id, input.uv).r;
	if (depth == 0.0)
		discard;

	float4 albedo = SampleTexture(albedo_tex_id, input.uv);
	float3 normal = unpackGBufferNormal(SampleTexture(normal_tex_id, input.uv, point_clamp_sampler).rgb);

	float4 shading = SampleTexture(shading_tex_id, input.uv);
	float metalness = shading.r;
	float roughness = saturate(shading.g);

	// IBL
	float3 world_pos = GetWSPosition(input.uv, depth);

	float3 v = normalize(camera_position.xyz - world_pos.rgb);
	float NdotV = saturate(dot(normal, v));

	float3 f0 = computeF0(albedo.rgb, metalness, shading.b);
	float3 diffuse_color = albedo.rgb * (1.0f - metalness);

	float3 irradiance = 0;
	if (irradiance_tex_id != 0)
		irradiance = irradiance_tex.Sample(linear_wrap_sampler, normal).rgb * sky_intensity;
	float3 ibl_diffuse = irradiance * diffuse_color / PI;

	float2 brdf_uv = float2(NdotV, 1.0 - roughness);
	
	Texture2D brdf_lut_tex = ResourceDescriptorHeap[brdf_lut_tex_id];
	float3 brdf_lut = brdf_lut_tex.Sample(linear_clamp_sampler, brdf_uv).rgb;

	float3 reflection = normalize(reflect(-v, normal));

	float3 prefilter = 0;
	if (prefilter_tex_id != 0)
	{
		uint mip_level, width, height, levels;
		prefilter_tex.GetDimensions(mip_level, width, height, levels);
		float lod = roughness * (float)levels;
		prefilter = prefilter_tex.SampleLevel(linear_wrap_sampler, reflection, lod).rgb * sky_intensity;
	}

	#if SSAO
		float ssao = SampleTexture(ssao_tex_id, input.uv).r;
	#else
		float ssao = 1.0f;
	#endif

	float specular_ao = computeSpecularAO(NdotV, ssao, roughness * roughness);

	float ibl_ambient_fallback_scale = 0.5;
	float ibl_specular_fallback_scale = 0.0;
	output.ambient = float4(ibl_diffuse * ssao * ibl_ambient_fallback_scale, 1.0);
	output.specular = float4(prefilter * (f0 * brdf_lut.x + brdf_lut.y) * specular_ao * ibl_specular_fallback_scale, 1.0);

	if (ddgi_volume_buffer_id > 0)
	{
		StructuredBuffer<DDGIVolume> volumes = ResourceDescriptorHeap[ddgi_volume_buffer_id];
		DDGIVolume volume = volumes[0];
		float volume_weight = GetVolumeWeight(world_pos, volume);
		if (volume_weight > 0.0f)
		{
			//output.ambient = float4(volume_weight, 0, 0, 1.0);

			DDGICascade cascade;
			uint cascade_index;
			GetCascadeForPosition(volume, cascade, cascade_index, world_pos);

			float3 cascade_spacing = cascade.spacing.xyz;
			float3 cascade_extent = volume.size.xyz * (cascade_spacing * 0.5);
			float3 cascade_center = cascade.min.xyz + cascade_extent;
			
			float view_distance = length(camera_position.xyz - world_pos);
			float cascade_blend_smooth = frac(max(view_distance - cascade_extent.x, 0) / cascade_spacing.x) * 0.1;
			float3 cascade_blend_point = world_pos - cascade_center - cascade_blend_smooth * cascade_spacing;
			
			const float blend_size = 0.5f;
			float fade_distance = cascade_spacing.x * blend_size;
			
			float3 extent_minus_blend_point = cascade_extent - abs(cascade_blend_point);
			float cascade_weight = saturate(min(extent_minus_blend_point.x, min(extent_minus_blend_point.y, extent_minus_blend_point.z)) / fade_distance);

			#define USE_CASCADES_BLENDING 1

			float3 surface_bias = GetSurfaceBias(normal, camera_position.xyz, world_pos, volume, cascade_index);
			float3 irradiance = SampleIrradiance(world_pos, normal, surface_bias, volume, cascade_index);

			#if USE_CASCADES_BLENDING == 1
				irradiance *= cascade_weight;

				// If not last cascade and blending needed
				if (cascade_index < volume.cascades_count - 1 && cascade_weight < 0.99f)
				{
					cascade_index++;
					float3 surface_bias = GetSurfaceBias(normal, camera_position.xyz, world_pos, volume, cascade_index);
					irradiance += SampleIrradiance(world_pos, normal, surface_bias, volume, cascade_index) * (1.0f - cascade_weight);
				}
			#endif

			output.ambient = float4((diffuse_color / PI) * irradiance * volume_weight * ssao, 1.0);
			//output.ambient = float4(cascade_weight, 0, 0, 1.0);
			//output.ambient = float4(cascade_index / 5.0, 0, 0, 1);
			//output.ambient = float4(irradiance, 1.0);
		}
	}

	#if SSR
		float3 ssr = textures[ssr_tex_id].Sample(linear_wrap_sampler, input.uv).rgb;
		ssr *= 1.0f - roughness;
		//output.specular += ssr;
	#endif

	return output;
}