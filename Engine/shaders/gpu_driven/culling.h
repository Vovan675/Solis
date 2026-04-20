#pragma once

struct MeshletCandidate
{
	uint instance_id;
	uint meshlet_id;
};

void transformBoundBox(inout float3 bound_center, inout float3 bound_extent, float4x4 world_transform)
{
	float3 translation = transpose(world_transform)[3].xyz;
	float3x3 rotation_scale = (float3x3)transpose(world_transform);
	
	bound_center = mul(bound_center, rotation_scale) + translation;
	bound_extent = mul(bound_extent, abs(rotation_scale));
}

float getScaleFromTransform(float4x4 transform)
{
	float3x3 rotation_scale = (float3x3)transpose(transform);
	float3 scale_factors = float3(
		length(rotation_scale[0]),
		length(rotation_scale[1]),
		length(rotation_scale[2])
	);
	return max(max(scale_factors.x, scale_factors.y), scale_factors.z);
}

void transformBoundSphere(inout float3 bound_center, inout float bound_radius, float4x4 world_transform)
{
	float3 translation = transpose(world_transform)[3].xyz;
	float3x3 rotation_scale = (float3x3)transpose(world_transform);

	bound_center = mul(bound_center, rotation_scale) + translation;
	bound_radius *= getScaleFromTransform(world_transform);
}

struct FrustumCullData
{
	float3 rect_min;
	float3 rect_max;
	bool is_visible;
};

FrustumCullData getFrustumCullDataOrtho(float3 bound_center, float3 bound_extent, float4x4 view_projection)
{
	FrustumCullData data;

	// Transform center to clip space
	float3 center_clip = mul(view_projection, float4(bound_center, 1)).xyz;

	// For orthographic projection, calculate the extent in clip space by transforming basis vectors
	float3 clip_delta = abs(bound_extent.x * mul(view_projection, float4(1, 0, 0, 0)).xyz)
	                  + abs(bound_extent.y * mul(view_projection, float4(0, 1, 0, 0)).xyz)
	                  + abs(bound_extent.z * mul(view_projection, float4(0, 0, 1, 0)).xyz);

	data.rect_min = center_clip - clip_delta;
	data.rect_max = center_clip + clip_delta;

	data.is_visible = data.rect_max.z < 1.0f;

	// Frustum side culling
	bool frustum_culled = any(data.rect_max.xy < -1.0f);
	data.is_visible = data.is_visible && !frustum_culled;

	return data;
}

FrustumCullData getFrustumCullData(float3 bound_center, float3 bound_extent, float4x4 view_projection)
{
	FrustumCullData data;

	data.rect_min = float3(1, 1, 1);
	data.rect_max = float3(-1, -1, -1);

	data.is_visible = true;

	float4 dx = mul(view_projection, float4(bound_extent.x * 2.0, 0, 0, 0));
	float4 dy = mul(view_projection, float4(0, bound_extent.y * 2.0, 0, 0));
	float4 dz = mul(view_projection, float4(0, 0, bound_extent.z * 2.0, 0));

	float min_w = 1e27f;
	float max_w = -1e27f;

	float4 planes_min = 1.0f;

	#define CALC_CORNER(p0, p1) \
		min_w = min(min_w, min(p0.w, p1.w)); \
		max_w = max(max_w, max(p0.w, p1.w)); \
		planes_min = min(planes_min, min(float4(p0.xy, -p0.xy) - p0.w, float4(p1.xy, -p1.xy) - p1.w)); \
		float3 screen_space_1 = p0.xyz / p0.w; \
		float3 screen_space_2 = p1.xyz / p1.w; \
		data.rect_min = min(data.rect_min, min(screen_space_1, screen_space_2)); \
		data.rect_max = max(data.rect_max, max(screen_space_1, screen_space_2));

	float4 p000 = mul(view_projection, float4(bound_center - bound_extent, 1));
	float4 p001 = p000 + dz;
	{
		CALC_CORNER(p000, p001);
	}

	float4 p100 = p000 + dx;
	float4 p101 = p001 + dx;
	{
		CALC_CORNER(p100, p101);
	}
	
	float4 p110 = p100 + dy;
	float4 p111 = p101 + dy;
	{
		CALC_CORNER(p110, p111);
	}

	float4 p010 = p110 - dx;
	float4 p011 = p111 - dx;
	{
		CALC_CORNER(p010, p011);
	}
	#undef CALC_CORNER

	// Camera between two corners (division by zero happens and conservatively fallback to full rect)
	if (min_w <= 0.0f && max_w > 0.0f)
	{
		data.rect_min = float3(-1, -1, -1);
		data.rect_max = float3(1, 1, 1);
		data.is_visible = true;
	} else if (max_w <= 0.0f)
	{
		// All corners behind camera - object is not visible
		data.is_visible = false;
	}

	bool frustum_culled = any(planes_min > 0.0f);
	data.is_visible = data.is_visible && !frustum_culled;

	return data;
}

// HIZ
float calculateHizMip(int4 pixel_rect, uint num_texels_to_sample)
{
	int2 rect_size = int2(pixel_rect.z - pixel_rect.x, pixel_rect.w - pixel_rect.y);
	
	//int2 mip_levels = ceil(log2(rect_size));
	//return max(mip_levels.x, mip_levels.y);
	int2 mip_levels = firstbithigh(rect_size);
	
	float mip_offset = log2((float)num_texels_to_sample) - 1;
	
	int mip = max(0, max(mip_levels.x, mip_levels.y) - mip_offset);
	
	int2 min_mip = pixel_rect.xy >> mip_levels;
	int2 max_mip = pixel_rect.zw >> mip_levels;
	int2 delta = max_mip - min_mip;

	int max_offset_in_pixels = num_texels_to_sample - 1;
	if (any(delta > max_offset_in_pixels))
		mip += 1;
	
	return mip;
}

//#define HIZ_OCCLUSION_DEBUG 1
bool isHizOcclusionCulled(FrustumCullData cull_data, float2 hiz_size, uint hiz_mips, Texture2D hiz_tex)
{
	// [-1, 1] to [0, 1]
	float4 rect_uv = saturate(float4(cull_data.rect_min.xy, cull_data.rect_max.xy) * float2(0.5, -0.5).xyxy + 0.5).xwzy;

	int2 min_hiz_pixel = (int2)(rect_uv.xy * hiz_size);
	int2 max_hiz_pixel = (int2)(rect_uv.zw * hiz_size);
	int4 pixel_rect = int4(min_hiz_pixel, max_hiz_pixel);

	#ifndef HIZ_SAMPLES
		#define HIZ_SAMPLES 4
	#endif
	
	float hiz_mip = calculateHizMip(pixel_rect, HIZ_SAMPLES);
	//hiz_mip += 1;
	
	float hiz_mip_scale = rcp(exp2(hiz_mip));
	float2 min_mip_pixel = float2(min_hiz_pixel) * hiz_mip_scale; 
	float2 max_mip_pixel = float2(max_hiz_pixel) * hiz_mip_scale;

	if (all(floor(min_mip_pixel) == floor(max_mip_pixel)))
	{
		hiz_mip -= 1;
	}
	
	if (hiz_mip < hiz_mips)
	{
		float hiz_depth = 0.0;

		#if HIZ_SAMPLES == 2
			float2 min_uv = rect_uv.xy;
			float4 depth = float4(
				hiz_tex.SampleLevel(point_clamp_sampler, min_uv, hiz_mip, int2(0, 0)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, min_uv, hiz_mip, int2(1, 0)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, min_uv, hiz_mip, int2(0, 1)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, min_uv, hiz_mip, int2(1, 1)).r
			);
			hiz_depth = min(min(depth.x, depth.y), min(depth.z, depth.w));
		#elif HIZ_SAMPLES == 4
			float2 base_uv = rect_uv.xy;
			
			float4 depth0 = float4(
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(0, 0)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(1, 0)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(2, 0)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(3, 0)).r
			);
			float4 depth1 = float4(
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(0, 1)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(1, 1)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(2, 1)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(3, 1)).r
			);
			float4 depth2 = float4(
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(0, 2)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(1, 2)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(2, 2)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(3, 2)).r
			);
			float4 depth3 = float4(
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(0, 3)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(1, 3)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(2, 3)).r,
				hiz_tex.SampleLevel(point_clamp_sampler, base_uv, hiz_mip, int2(3, 3)).r
			);
			
			float4 depth = min(min(depth0, depth1), min(depth2, depth3));
			hiz_depth = min(min(depth.x, depth.y), min(depth.z, depth.w));
		#endif

		bool is_culled = cull_data.rect_max.z < hiz_depth;

		#if HIZ_OCCLUSION_DEBUG
			float3 instance_color = is_culled ? float3(1, 0, 0) : float3(0, 1, 0);
			addScreenQuad(((rect_uv.xy) * 2 - 1) * float2(1, -1), ((rect_uv.zw) * 2 - 1) * float2(1, -1), instance_color);
		#endif
		return is_culled;
	}

	return false;
}