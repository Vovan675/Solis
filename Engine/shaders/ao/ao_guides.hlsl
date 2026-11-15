#include "ao_common.h"

cbuffer Constants : register(b0)
{
	uint4 view_depth_mip_tex_ids; // Mips 1-4
	uint depth_tex_id;
	uint normal_tex_id;
	uint view_depth_mip0_tex_id;
	uint normal_guide_tex_id;
	int resolution_scale;
};

static const int GROUP_SIZE = 16;

static Texture2D<float> depth_texture = ResourceDescriptorHeap[depth_tex_id];
[[vk::image_format("r32f")]]
static RWTexture2D<float> view_depth_mip0 = ResourceDescriptorHeap[view_depth_mip0_tex_id];
#if WRITE_NORMAL
	static Texture2D normal_texture = ResourceDescriptorHeap[normal_tex_id];
	[[vk::image_format("rgba8")]]
	static RWTexture2D<float4> normal_guide = ResourceDescriptorHeap[normal_guide_tex_id];
#endif

#if WRITE_MIPS
	[[vk::image_format("r32f")]]
	static RWTexture2D<float> view_depth_mip1 = ResourceDescriptorHeap[view_depth_mip_tex_ids.x];
	[[vk::image_format("r32f")]]
	static RWTexture2D<float> view_depth_mip2 = ResourceDescriptorHeap[view_depth_mip_tex_ids.y];
	[[vk::image_format("r32f")]]
	static RWTexture2D<float> view_depth_mip3 = ResourceDescriptorHeap[view_depth_mip_tex_ids.z];
	[[vk::image_format("r32f")]]
	static RWTexture2D<float> view_depth_mip4 = ResourceDescriptorHeap[view_depth_mip_tex_ids.w];

	groupshared float shared_view_z[GROUP_SIZE][GROUP_SIZE];

	float getMinViewZ(uint2 corner, uint step)
	{
		float top = min(shared_view_z[corner.y][corner.x], shared_view_z[corner.y][corner.x + step]);
		float bottom = min(shared_view_z[corner.y + step][corner.x], shared_view_z[corner.y + step][corner.x + step]);
		return min(top, bottom);
	}

	void writeViewDepthMip(int mip, uint2 pixel, float view_z)
	{
		if (mip == 1)
			view_depth_mip1[pixel] = view_z;
		else if (mip == 2)
			view_depth_mip2[pixel] = view_z;
		else if (mip == 3)
			view_depth_mip3[pixel] = view_z;
		else
			view_depth_mip4[pixel] = view_z;
	}
#endif

static int2 resolution = getScaledRenderResolution(resolution_scale);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID, uint2 group_thread_id : SV_GroupThreadID)
{
	int2 clamped_pixel = min(int2(pixel), resolution - 1);
	int2 gbuffer_pixel = clamped_pixel * resolution_scale;

	float depth = depth_texture[gbuffer_pixel];
	float view_z = depth == 0.0 ? z_far : -GetVSPosition(getScaledPixelUV(clamped_pixel, resolution_scale), depth).z;

	if (all(pixel < uint2(resolution)))
	{
		view_depth_mip0[pixel] = view_z;
		#if WRITE_NORMAL
			normal_guide[pixel] = normal_texture[gbuffer_pixel];
		#endif
	}

	// Calculate mips one after one with group barriers between
	#if WRITE_MIPS
		shared_view_z[group_thread_id.y][group_thread_id.x] = view_z;
		
		[unroll]
		for (int mip = 1; mip <= 4; mip++)
		{
			GroupMemoryBarrierWithGroupSync();

			uint texel_size = 1u << mip;
			if (all(group_thread_id % texel_size == 0)) // Ony n-th (2nd, 4th etc thread writes mip)
			{
				float nearest_view_z = getMinViewZ(group_thread_id, texel_size / 2);
				shared_view_z[group_thread_id.y][group_thread_id.x] = nearest_view_z;

				uint2 mip_pixel = pixel >> mip;
				uint2 mip_resolution = max(resolution >> mip, 1);
				if (all(mip_pixel < mip_resolution))
					writeViewDepthMip(mip, mip_pixel, nearest_view_z);
			}
		}
	#endif
}
