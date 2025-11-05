#include "common.h"
#include "bindless.h"

struct VSInput {
	float2 uv : TEXCOORD0;
};

struct PSOutput {
	float4 color : SV_Target;
};

cbuffer UBO : register(b0)
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
	uint ddgi_distance_tex_id;
	uint ddgi_irradiance_tex_id;
	uint ddgi_metadata_tex_id;
};

PSOutput PSMain(VSInput input)
{
	PSOutput output;
	uint mode = present_mode;

	float2 uv = input.uv;
	float4 value = float4(1.0f, 1.0f, 1.0f, 1.0f);
	const float border = 0.001f;

	if (mode == 0)
	{
		if (uv.x < 1.0f / 3.0f - border)
		{
			mode = 1;
		}
		else if (uv.x > 1.0f / 3.0f && uv.x < 2.0f / 3.0f)
		{
			mode = 2;
		}
		else
		{
			mode = 3;
		}
		uv.x *= 3.0f;
	}

	float4 composite_final = SampleTexture(composite_final_tex_id, uv);
	float4 albedo = SampleTexture(albedo_tex_id, uv);
	float4 shading = SampleTexture(shading_tex_id, uv);
	float4 normal = SampleTexture(normal_tex_id, uv);
	float depth = SampleTexture(depth_tex_id, uv).r;
	float3 diffuse = SampleTexture(light_diffuse_id, uv).rgb;
	float3 specular = SampleTexture(light_specular_id, uv).rgb;
	float2 brdf_lut = SampleTexture(brdf_lut_id, uv).xy;
	float ssao = SampleTexture(ssao_id, uv).r;

	switch (mode)
	{
		case 1:
			value = composite_final;
			break;
		case 2:
			value = albedo;
			break;
		case 3:
			value = float4(shading.r, shading.r, shading.r, 1.0f);
			break;
		case 4:
			value = float4(shading.g, shading.g, shading.g, 1.0f);
			break;
		case 5:
			value = float4(shading.b, shading.b, shading.b, 1.0f);
			break;
		case 6:
			value = normal * 2.0f - 1.0f;
			break;
		case 7:
			value = float4(depth, depth, depth, 1.0f);
			break;
		case 8:
		{
			float3 view_pos = GetVSPosition(uv, depth);
			float3 world_pos = GetWSPosition(uv, depth);

			value = float4(world_pos.xyz, 1.0f);
			break;
		}
		case 9:
			value = float4(diffuse, 1.0f);
			break;
		case 10:
			value = float4(specular, 1.0f);
			break;
		case 11:
			value = float4(brdf_lut, 0.0f, 1.0f);
			break;
		case 12:
			value = float4(ssao, ssao, ssao, 1.0f);
			break;
		case 13:
			value = composite_final;

			const float scale = 1.0f;
			const float offset = 0.02f;
			const float max_height = 0.4f;
			float screen_aspect = swapchain_size.x / float(swapchain_size.y);
			float y_pos = 0.0f;

			Texture2D dist_tex = ResourceDescriptorHeap[ddgi_distance_tex_id];
			int2 dist_dim;
			dist_tex.GetDimensions(dist_dim.x, dist_dim.y);
			float dist_aspect = float(dist_dim.x) / float(dist_dim.y);
			float dist_h = screen_aspect / dist_aspect * scale;
			float dist_w = scale;
			if (dist_h > max_height) {
				dist_w *= max_height / dist_h;
				dist_h = max_height;
			}
			if (uv.x <= dist_w && uv.y <= dist_h) {
				float2 dist_uv = float2(uv.x / dist_w, uv.y / dist_h);
				float2 dist = SampleTexture(ddgi_distance_tex_id, dist_uv, point_clamp_sampler).rg;
				value = float4(dist.x, dist.y, 0, 1);
			}

			Texture2D irr_tex = ResourceDescriptorHeap[ddgi_irradiance_tex_id];
			int2 irr_dim;
			irr_tex.GetDimensions(irr_dim.x, irr_dim.y);
			float irr_aspect = float(irr_dim.x) / float(irr_dim.y);
			float irr_h = screen_aspect / irr_aspect * scale;
			float irr_w = scale;
			if (irr_h > max_height) {
				irr_w *= max_height / irr_h;
				irr_h = max_height;
			}
			y_pos = dist_h + offset;
			if (uv.x <= irr_w && uv.y >= y_pos && uv.y <= y_pos + irr_h) {
				float2 irr_uv = float2(uv.x / irr_w, (uv.y - y_pos) / irr_h);
				float4 irr = SampleTexture(ddgi_irradiance_tex_id, irr_uv, point_clamp_sampler);
				value = float4(irr.rgb, 1.0f);
			}

			Texture2D meta_tex = ResourceDescriptorHeap[ddgi_metadata_tex_id];
			int2 meta_dim;
			meta_tex.GetDimensions(meta_dim.x, meta_dim.y);
			float meta_aspect = float(meta_dim.x) / float(meta_dim.y);
			float meta_h = screen_aspect / meta_aspect * scale;
			float meta_w = scale;
			if (meta_h > max_height) {
				meta_w *= max_height / meta_h;
				meta_h = max_height;
			}
			y_pos += irr_h + offset;
			if (uv.x <= meta_w && uv.y >= y_pos && uv.y <= y_pos + meta_h) {
				float2 meta_uv = float2(uv.x / meta_w, (uv.y - y_pos) / meta_h);
				float4 meta = SampleTexture(ddgi_metadata_tex_id, meta_uv, point_clamp_sampler);
				value = float4(meta.rgba);
			}
			break;
	}

	output.color = float4(LinearToSRGB(value.rgba).rgb, 1.0f);
	return output;
}
