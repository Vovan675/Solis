#include "ao_common.h"

#define ANTILAG_NONE 0
#define ANTILAG_LOCAL_MEAN 1
#define ANTILAG_FAST_HISTORY 2

cbuffer Constants : register(b0)
{
	uint noisy_tex_id;
	uint history_tex_id;
	uint view_depth_tex_id;
	uint motion_vectors_tex_id;
	uint accumulated_tex_id;
	uint reset_history;
	float max_history_length;
	int resolution_scale;
	uint output_tex_id;
	float antilag_window_scale;
};

static Texture2D<float> noisy_texture = ResourceDescriptorHeap[noisy_tex_id];
static Texture2D<float4> history_texture = ResourceDescriptorHeap[history_tex_id];
static Texture2D<float> view_depth_texture = ResourceDescriptorHeap[view_depth_tex_id];
static Texture2D<float2> motion_vectors_texture = ResourceDescriptorHeap[motion_vectors_tex_id];
[[vk::image_format("rgba16f")]]
static RWTexture2D<float4> accumulated_texture = ResourceDescriptorHeap[accumulated_tex_id];
#if WRITE_OUTPUT
	[[vk::image_format("r8")]]
	static RWTexture2D<float> output_texture = ResourceDescriptorHeap[output_tex_id];
#endif

static int2 resolution = getScaledRenderResolution(resolution_scale);

static const float DISOCCLUSION_DEPTH_THRESHOLD = 0.02;
static const float ANTILAG_RESET_LENGTH = 4.0;
static const float ANTILAG_RESET_SENSITIVITY = 4.0;
static const float MAX_FAST_HISTORY_LENGTH = 4.0;

struct Reprojection
{
	float ao;
	float length;
	#if ANTILAG_MODE == ANTILAG_FAST_HISTORY
		float fast_ao;
	#endif
};

// History: x = ao, y = view z, z = history length, w = fast history ao
Reprojection reproject(uint2 pixel, float2 uv, float3 position)
{
	float2 old_uv = uv + motion_vectors_texture[pixel * resolution_scale];
	float2 old_pixel = (old_uv * render_resolution.xy - 0.5) / resolution_scale;
	float expected_old_view_z = -mul(old_view, float4(position, 1.0)).z;

	int2 base_pixel = floor(old_pixel);
	float4 bilinear_weights = getBilinearWeights(frac(old_pixel));

	Reprojection output = (Reprojection)0;
	float weight_sum = 0.0;
	[unroll]
	for (int i = 0; i < 4; i++)
	{
		// offsets (0, 0), (1, 0), (0, 1), (1, 1)
		int2 sample_pixel = base_pixel + int2(i & 1, i >> 1);
		float4 sample_history = history_texture[sample_pixel];

		float sample_weight = bilinear_weights[i];

		// Outside of the screen
		sample_weight *= all(sample_pixel >= 0) && all(sample_pixel < resolution);

		// Disocclusion by depth
		sample_weight *= abs(sample_history.y - expected_old_view_z) < DISOCCLUSION_DEPTH_THRESHOLD * expected_old_view_z;

		output.ao += sample_history.x * sample_weight;
		output.length += sample_history.z * sample_weight;
		#if ANTILAG_MODE == ANTILAG_FAST_HISTORY
			output.fast_ao += sample_history.w * sample_weight;
		#endif
		weight_sum += sample_weight;
	}
	if (weight_sum < 0.01)
		return (Reprojection)0;

	output.ao /= weight_sum;
	output.length /= weight_sum;
	#if ANTILAG_MODE == ANTILAG_FAST_HISTORY
		output.fast_ao /= weight_sum;
	#endif
	return output;
}

struct LocalMoments
{
	float mean;
	float std_dev; // Standard Deviation (https://en.wikipedia.org/wiki/Standard_deviation)
};

// LDS samples
static const int GROUP_SIZE = 8;
static const int MOMENTS_RADIUS = 2; // Radius like in spatial filter
static const int SHARED_SIZE = GROUP_SIZE + 2 * MOMENTS_RADIUS;

groupshared float shared_ao[SHARED_SIZE][SHARED_SIZE];

void fillShared(uint2 group_id, int thread_index)
{
	int2 origin = int2(group_id) * GROUP_SIZE - MOMENTS_RADIUS;
	for (int i = thread_index; i < SHARED_SIZE * SHARED_SIZE; i += GROUP_SIZE * GROUP_SIZE)
	{
		int2 offset = int2(i % SHARED_SIZE, i / SHARED_SIZE);
		int2 sample_pixel = clamp(origin + offset, 0, resolution - 1);
		
		shared_ao[offset.y][offset.x] = noisy_texture[sample_pixel];
	}
}

// Calculate mean value around radius and standard deviation inside it
LocalMoments getLocalMoments(int2 group_thread_id)
{
	int2 center = group_thread_id + MOMENTS_RADIUS;
	float sum = 0.0;
	float sum_squared = 0.0;
	[unroll]
	for (int y = -MOMENTS_RADIUS; y <= MOMENTS_RADIUS; y++)
	{
		[unroll]
		for (int x = -MOMENTS_RADIUS; x <= MOMENTS_RADIUS; x++)
		{
			float ao = shared_ao[center.y + y][center.x + x];
			sum += ao;
			sum_squared += ao * ao;
		}
	}
	float count = (2 * MOMENTS_RADIUS + 1) * (2 * MOMENTS_RADIUS + 1);

	float mean = sum / count;
	float mean_squared = sum_squared / count;

	LocalMoments moments;
	moments.mean = mean;
	moments.std_dev = sqrt(max(0.0, mean_squared - mean * mean));
	return moments;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID, uint2 group_id : SV_GroupID, uint2 group_thread_id : SV_GroupThreadID, uint thread_index : SV_GroupIndex)
{
	#if ANTILAG_MODE != ANTILAG_NONE
		fillShared(group_id, thread_index);
		GroupMemoryBarrierWithGroupSync();
	#endif

	if (any(pixel >= uint2(resolution)))
		return;

	float view_z = view_depth_texture[pixel];
	if (view_z >= z_far)
	{
		accumulated_texture[pixel] = 0.0;
		#if WRITE_OUTPUT
			output_texture[pixel] = 1.0;
		#endif
		return;
	}

	float2 uv = getScaledPixelUV(pixel, resolution_scale);
	float3 position = mul(iview, float4(GetVSPositionFromViewZ(uv, view_z), 1.0)).xyz;
	float current_ao = noisy_texture[pixel];

	// Reprojection
	Reprojection reprojection = reproject(pixel, uv, position);

	float history_ao = reprojection.ao;
	float history_length = reset_history ? 1.0 : min(reprojection.length + 1.0, max_history_length);
	float fast_ao = 0.0;

	// Anti lag (compare to local area in current noisy output, clamp to its deviation and use as reference)
	#if ANTILAG_MODE != ANTILAG_NONE
		LocalMoments moments = getLocalMoments(group_thread_id);
		
		#if ANTILAG_MODE == ANTILAG_LOCAL_MEAN
			// Reference is just a mean
			float reference = moments.mean;
			float reference_std_dev = max(moments.std_dev / 5.0, 0.02); // std dev of a 25 samples mean
		#elif ANTILAG_MODE == ANTILAG_FAST_HISTORY
			// Reference is accumulated ao with short history
			float fast_length = min(history_length, MAX_FAST_HISTORY_LENGTH);
			fast_ao = lerp(reprojection.fast_ao, current_ao, 1.0 / fast_length);

			float reference = fast_ao;
			float reference_std_dev = max(moments.std_dev / sqrt(fast_length), 0.02);
		#endif

		// Clamp history to be around reference. The more deviation (noisy), the more allowed window
		float window = antilag_window_scale * reference_std_dev;
		float clamped_ao = clamp(history_ao, reference - window, reference + window);

		// If clamped_ao is far from history_ao, shorten the history
		float clamp_distance = abs(clamped_ao - history_ao);
		float reset_amount = saturate(ANTILAG_RESET_SENSITIVITY * clamp_distance);
		history_length = lerp(history_length, min(history_length, ANTILAG_RESET_LENGTH), reset_amount);

		history_ao = clamped_ao;
	#endif

	// Accumulation
	float accumulated_ao = lerp(history_ao, current_ao, 1.0 / history_length);
	//accumulated_ao = history_length / max_history_length;

	accumulated_texture[pixel] = float4(accumulated_ao, view_z, history_length, fast_ao);
	#if WRITE_OUTPUT
		output_texture[pixel] = accumulated_ao;
	#endif
}
