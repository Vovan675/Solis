#pragma once
#include "EngineMath.h"

static const int MAX_FRAMES_IN_FLIGHT = 2;

enum ShaderType
{
	// Default pipeline
	VERTEX_SHADER,
	FRAGMENT_SHADER,
	// Compute pipeline
	COMPUTE_SHADER,
	// Ray tracing pipeline
	RAY_GENERATION_SHADER,
	MISS_SHADER,
	CLOSEST_HIT_SHADER,
};

enum BufferUsage
{
	VERTEX_BUFFER = 1 << 0,
	INDEX_BUFFER = 1 << 1,
	UNIFORM_BUFFER = 1 << 2,
};

struct BufferDescription
{
	uint64_t size;
	bool useStagingBuffer;
	VkBufferUsageFlags bufferUsageFlags;
	uint32_t usage;
	uint64_t alignment = 0;

	uint32_t vertex_buffer_stride = 0;
};

enum Filter
{
	FILTER_NEAREST,
	FILTER_LINEAR
};

enum SamplerMode
{
	SAMPLER_MODE_REPEAT,
	SAMPLER_MODE_CLAMP_TO_EDGE,
	SAMPLER_MODE_CLAMP_TO_BORDER
};

enum SampleCount
{
	SAMPLE_COUNT_1,
	SAMPLE_COUNT_2,
	SAMPLE_COUNT_4,
	SAMPLE_COUNT_8,
	SAMPLE_COUNT_16,
	SAMPLE_COUNT_32,
	SAMPLE_COUNT_64,
};

enum Format
{
	FORMAT_UNDEFINED = 0,

	// 8 bit
	FORMAT_R8_UNORM,
	FORMAT_R8G8_UNORM,
	FORMAT_R8G8B8A8_UNORM,
	FORMAT_R8G8B8A8_SRGB,

	// 16 bit
	FORMAT_R16_UNORM,
	FORMAT_R16G16_UNORM,
	FORMAT_R16G16_SFLOAT,
	FORMAT_R16G16B16A16_UNORM,

	// 32 bit
	FORMAT_R32_SFLOAT,
	FORMAT_R32G32_SFLOAT,
	FORMAT_R32G32B32_SFLOAT,
	FORMAT_R32G32B32A32_SFLOAT,

	// depth stencil
	FORMAT_D32S8,

	// combined
	FORMAT_R11G11B10_UFLOAT,

	// compressed
	FORMAT_BC1,
	FORMAT_BC3,
	FORMAT_BC5,
	FORMAT_BC7,
};

enum TextureUsageFlags : uint32_t
{
	TEXTURE_USAGE_TRANSFER_SRC = 1 << 1,
	TEXTURE_USAGE_TRANSFER_DST = 1 << 2,
	TEXTURE_USAGE_NO_SAMPLED = 1 << 3,
	TEXTURE_USAGE_STORAGE = 1 << 4,
	TEXTURE_USAGE_ATTACHMENT = 1 << 5,
};

struct TextureDescription
{
	bool is_cube = false;
	uint32_t width;
	uint32_t height;
	uint32_t mip_levels = 1;
	uint32_t array_levels = 1;
	SampleCount sample_count = SAMPLE_COUNT_1;
	Format format;
	uint32_t usage_flags = 0;
	SamplerMode sampler_mode = SAMPLER_MODE_REPEAT;
	Filter filtering = FILTER_LINEAR;
	bool anisotropy = false;
	bool use_comparison_less = false;

	size_t getHash() const
	{
		size_t hash = 0;
		Engine::Math::hash_combine(hash, is_cube);
		Engine::Math::hash_combine(hash, width);
		Engine::Math::hash_combine(hash, height);
		Engine::Math::hash_combine(hash, mip_levels);
		Engine::Math::hash_combine(hash, array_levels);
		Engine::Math::hash_combine(hash, sample_count);
		Engine::Math::hash_combine(hash, format);
		Engine::Math::hash_combine(hash, usage_flags);
		Engine::Math::hash_combine(hash, sampler_mode);
		Engine::Math::hash_combine(hash, filtering);
		Engine::Math::hash_combine(hash, anisotropy);
		Engine::Math::hash_combine(hash, use_comparison_less);

		return hash;
	}
};

enum RESOURCE_TYPE
{
	RESOURCE_TYPE_SAMPLER,
	RESOURCE_TYPE_IMAGE_VIEW,
	RESOURCE_TYPE_IMAGE,
	RESOURCE_TYPE_MEMORY,
	RESOURCE_TYPE_BUFFER,
	RESOURCE_TYPE_PIPELINE,
	RESOURCE_TYPE_ROOT_SIGNATURE,
	RESOURCE_TYPE_PIPELINE_LAYOUT,
};