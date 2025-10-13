#include "bindless.h"

struct VSInput {
	float2 uv : TEXCOORD0;
};

struct PSOutput {
	float4 color : SV_Target;
};

cbuffer UBO : register(b0)
{
	uint composite_final_tex_id;
	uint normal_tex_id;
	uint shading_tex_id;
	uint depth_tex_id;
};

static Texture2D<float> depth_tex = ResourceDescriptorHeap[depth_tex_id];

float2 ViewPosToUV(float3 view_pos)
{
	float4 pp = mul(projection, float4(view_pos, 1.0));
	float2 ndc = pp.xy / pp.w;

	return float2(0.5, -0.5) * ndc + float2(0.5, 0.5);
}

#define BINARY_SEARCH_STEPS 16
#define MAX_STEPS 64
#define MAX_ROUGHNESS 0.7

// Returns reflection color in RGB, fade in Alpha
float4 CastSingleRay(float3 view_pos, float3 view_reflection, float roughness)
{
	bool was_under = false;
	bool hit = false;
	float bias = 0.1;
	
	float max_dist = abs(view_pos.z) * rsqrt(0.01 + saturate(dot(view_reflection.xy, view_reflection.xy)));
	max_dist += bias;
	float step = max_dist / float(MAX_STEPS);
	float refine_step = step / float(BINARY_SEARCH_STEPS);

	float2 tc;
	for(float t = step + bias; t <= max_dist && !hit; t += step)
	{
		float3 sample_pos = view_pos + t * view_reflection;
		//convert to screen texture coordinates
		tc = ViewPosToUV(sample_pos);

		float depth = GetVSPosition(tc.xy, depth_tex.Sample(point_clamp_sampler, tc.xy).x).z;
		bool under = sample_pos.z > depth;
		if(!was_under && under)
		{
			float t_ref = t - step;
			float local_best = sample_pos.z - depth;
			for(int i = 0; i < BINARY_SEARCH_STEPS; ++i)
			{
				t_ref += refine_step;
				float3 p2 = view_pos + t_ref * view_reflection;
				float2 tc2 = ViewPosToUV(p2);

				float d2 = GetVSPosition(tc2.xy, depth_tex.Sample(point_clamp_sampler, tc2.xy).x).z;
				float diff = p2.z - d2;

				if(diff >= 0.0 && diff < local_best)
				{
					local_best = diff;
					tc = tc2;
				}
			}

			hit = local_best < (0.25 * step);
		}
		was_under = under;
	}

	if (!hit)
		return 0;

	//snap texcoords to nearest texel
	float2 hit_uv = (floor(tc * swapchain_size.xy) + float2(0.5,0.5)) * swapchain_size.zw;

	float3 reflection_color = SampleTexture(composite_final_tex_id, hit_uv, point_clamp_sampler).rgb;

	float2 edge = saturate(10.0 - abs(hit_uv - 0.5) * 20.0);
	float edge_fade = min(edge.x, edge.y);

	// TODO: add roughness fade
	float roughness_fade = saturate(1.0f - (roughness / MAX_ROUGHNESS));
	roughness_fade *= roughness_fade;

	return float4(reflection_color, edge_fade * roughness_fade);
}


float3 CalculateSSR(float2 uv)
{
	float3 original = SampleTexture(composite_final_tex_id, uv, point_clamp_sampler).rgb;
	//original *= 0;

	float scene_depth = SampleTexture(depth_tex_id, uv, point_clamp_sampler).r;
	if (scene_depth == 1.0)
		return original;

	float3 packed_normal = SampleTexture(normal_tex_id, uv, point_clamp_sampler).rgb;
	float3 normal_ws = normalize(packed_normal * 2.0f - 1.0f);
	float3 normal_vs = normalize(mul((float3x3)view, normal_ws));

	float3 view_pos = GetVSPosition(uv, scene_depth);
	float3 view_reflection = normalize(reflect(normalize(view_pos), normal_vs));

	// Early out for reflection faced the camera
	if (view_reflection.z < -0.97)
		return original;

	float roughness = SampleTexture(shading_tex_id, uv, point_clamp_sampler).g;

	// Early out for very rough surfaces
	if (roughness >= MAX_ROUGHNESS)
		return original;

	// TODO: Add multiple samples and average them
	float4 hit_result = CastSingleRay(view_pos, view_reflection, roughness);

	float3 reflection_color = hit_result.rgb * hit_result.a;
	return original + reflection_color;
}

float4 PSMain(VSInput input) : SV_TARGET
{
	float3 result = CalculateSSR(input.uv);
	return float4(result, 1);
}
