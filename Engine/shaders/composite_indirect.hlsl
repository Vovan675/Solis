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
	uint ambient_occlusion_tex_id;
	uint ssr_tex_id;
	float ambient_occlusion_intensity;
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

	#if AMBIENT_OCCLUSION
		float ambient_occlusion = lerp(1.0f, SampleTexture(ambient_occlusion_tex_id, input.uv).r, ambient_occlusion_intensity);
	#else
		float ambient_occlusion = 1.0f;
	#endif

	float specular_ao = computeSpecularAO(NdotV, ambient_occlusion, roughness * roughness);

	float ibl_ambient_fallback_scale = 0.5;
	float ibl_specular_fallback_scale = 0.0;
	output.ambient = float4(ibl_diffuse * ambient_occlusion * ibl_ambient_fallback_scale, 1.0);
	output.specular = float4(prefilter * (f0 * brdf_lut.x + brdf_lut.y) * specular_ao * ibl_specular_fallback_scale, 1.0);

	#if DDGI
	{
		DDGIVolume volume = ddgi_volume;
		float volume_weight = GetVolumeWeight(world_pos, volume);
		if (volume_weight > 0.0f)
		{
			float3 irradiance = SampleCascadesIrradiance(world_pos, normal, camera_position.xyz, volume);
			output.ambient = float4((diffuse_color / PI) * irradiance * volume_weight * ambient_occlusion, 1.0);
			#if 1
				// Use cascade irradiance as specular approximation. Ideally only for rough reflections
				output.specular = float4((irradiance / PI) * (f0 * brdf_lut.x + brdf_lut.y) * volume_weight * specular_ao, 1.0);
			#endif
		}
	}
	#endif

	#if SSR
		float3 ssr = textures[ssr_tex_id].Sample(linear_wrap_sampler, input.uv).rgb;
		ssr *= 1.0f - roughness;
		//output.specular += ssr;
	#endif

	return output;
}