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
	uint hiz_tex_id;
	uint debug_tex_srv_id;
	uint overdraw_tex_srv_id;
};

void ComputePreviewSize(float2 tex_size, float screen_aspect, float scale, float max_h, out float2 size)
{
    float aspect = tex_size.x / tex_size.y;
    float h = screen_aspect / aspect * scale;
    float w = scale;

    if (h > max_h) {
        w *= max_h / h;
        h = max_h;
    }

	size.x = w;
	size.y = h;
}

bool DrawPreview(float2 uv, float2 pos, float2 size, int tex_id, int layer, out float4 color)
{
	color = 0;

	float2 uv0 = pos;
    float2 uv1 = pos + size;

    if (uv.x < uv0.x || uv.x > uv1.x ||
        uv.y < uv0.y || uv.y > uv1.y)
        return false;

    float2 tuv = float2(
        (uv.x - uv0.x) / size.x,
        (uv.y - uv0.y) / size.y
    );

    if (layer < 0)
    {
        color = SampleTexture(tex_id, tuv, point_clamp_sampler);
    }
    else
    {
        color = SampleTextureArray(tex_id, float3(tuv, layer), point_clamp_sampler);
    }
	return true;
}

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
		{
			value = composite_final;

			const float scale = 1.0f;
			const float offset = 0.02f;
			const float max_height = 0.1f;
			float screen_aspect = swapchain_size.x / float(swapchain_size.y);
			float y_pos = 0.0f;
			float max_cascade = 6;

			Texture2DArray dist_tex = ResourceDescriptorHeap[ddgi_distance_tex_id];
			int3 dist_dim;
			dist_tex.GetDimensions(dist_dim.x, dist_dim.y, dist_dim.z);

			float2 size;
			ComputePreviewSize(dist_dim.xy, screen_aspect, scale, max_height, size);

			float4 preview_color;

			for (int i = 0; i < min(max_cascade, dist_dim.z); i++)
			{
				if (DrawPreview(uv, float2(0, y_pos), size, ddgi_distance_tex_id, i, preview_color))
				{
					value = float4(preview_color.rg, 0, 1);
					//value = float4(1, 0, 0, 1);
				}
				y_pos += size.y + offset;
			}
			y_pos += 0.03;

			Texture2DArray irr_tex = ResourceDescriptorHeap[ddgi_irradiance_tex_id];
			int3 irr_dim;
			irr_tex.GetDimensions(irr_dim.x, irr_dim.y, irr_dim.z);

			ComputePreviewSize(irr_dim.xy, screen_aspect, scale, max_height, size);

			for (int i = 0; i < min(max_cascade, irr_dim.z); i++)
			{
				if (DrawPreview(uv, float2(0, y_pos), size, ddgi_irradiance_tex_id, i, preview_color))
				{
					value = float4(preview_color.rgb, 1);
					//value = float4(0, 1, 0, 1);
				}
				y_pos += size.y + offset;
			}
			y_pos += 0.03;

			Texture2DArray meta_tex = ResourceDescriptorHeap[ddgi_metadata_tex_id];
			int3 meta_dim;
			irr_tex.GetDimensions(meta_dim.x, meta_dim.y, meta_dim.z);

			ComputePreviewSize(meta_dim.xy, screen_aspect, scale, max_height, size);

			for (int i = 0; i < min(max_cascade, meta_dim.z); i++)
			{
				if (DrawPreview(uv, float2(0, y_pos), size, ddgi_metadata_tex_id, i, preview_color))
				{
					value = float4(preview_color.rgb, 1);
				}
				y_pos += size.y + offset;
			}
			y_pos += 0.03;

			/*
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
				*/
		}
		break;
		case 14:
		{
			Texture2D tex = ResourceDescriptorHeap[hiz_tex_id];
			uint width, height, mip_count;
			tex.GetDimensions(0, width, height, mip_count);
			uint level = abs(sin(time)) * mip_count;
			level = 4;
			float3 hiz = tex.SampleLevel(point_wrap_sampler, uv, level).rrr;
			value = float4(hiz, 1);
		}
		break;
		case 15:
		{
			Texture2D tex = ResourceDescriptorHeap[debug_tex_srv_id];
			float3 v = tex.Sample(point_wrap_sampler, uv).rgb;
			//value = float4(v, 1);
			value = float4(v, 1) + 0.01 * albedo;
		}
		break;
		case 16:
		{
			Texture2D tex = ResourceDescriptorHeap[overdraw_tex_srv_id];
			uint v = tex.Sample(point_wrap_sampler, uv).r;
			//value = float4(v, 1);
			value = float4(v / 10.0f, 0, 0, 1);
		}
		break;
	}

	output.color = float4(LinearToSRGB(value.rgba).rgb, 1.0f);
	return output;
}
