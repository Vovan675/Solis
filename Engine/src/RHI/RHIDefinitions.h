#pragma once
#include "Core/Core.h"
#include "Math/EngineMath.h"

#define ENABLE_RHI_VALIDATION

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
	UAV_BUFFER = 1 << 3,
	STORAGE_BUFFER = 1 << 4,
	RAW_STORAGE_BUFFER = 1 << 5,
	ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER = 1 << 6,
	ACCELERATION_STRUCTURE_STORAGE_BUFFER = 1 << 7,
	SHADER_BINGING_TABLE_BUFFER = 1 << 8,
};

struct BufferDescription
{
	uint64_t size = 0;
	bool useStagingBuffer;
	uint32_t usage = 0;
	uint32_t alignment = 0;

	uint32_t stride = 0;
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
	SAMPLE_COUNT_64
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
	FORMAT_R16G16B16A16_SFLOAT,

	// 32 bit
	FORMAT_R32_UINT,
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

inline uint32_t getFormatSize(Format format)
{
	switch (format)
	{
		// 8 bit
		case FORMAT_R8_UNORM: return 1;
		case FORMAT_R8G8_UNORM: return 2;
		case FORMAT_R8G8B8A8_UNORM:
		case FORMAT_R8G8B8A8_SRGB: return 4;

		// 16 bit
		case FORMAT_R16_UNORM: return 2;
		case FORMAT_R16G16_UNORM: return 4;
		case FORMAT_R16G16B16A16_UNORM: return 8;
		case FORMAT_R16G16B16A16_SFLOAT: return 8;

		// 32 bit
		case FORMAT_R32_UINT:
		case FORMAT_R32_SFLOAT: return 4;
		case FORMAT_R32G32_SFLOAT: return 8;
		case FORMAT_R32G32B32_SFLOAT: return 16;
		case FORMAT_R32G32B32A32_SFLOAT: return 16;

		// depth stencil
		case FORMAT_D32S8: return 5;

		// combined
		case FORMAT_R11G11B10_UFLOAT: return 4;

		// compressed
		case FORMAT_BC1: return 0;
		case FORMAT_BC3: return 0;
		case FORMAT_BC5: return 0;
		case FORMAT_BC7: return 0;
	}
	return 0;
}

inline const char *getFormatName(Format format)
{
	switch (format)
	{
		case FORMAT_R8_UNORM: return "R8_UNORM";
		case FORMAT_R8G8_UNORM: return "R8G8_UNORM";
		case FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
		case FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
		case FORMAT_R16_UNORM: return "R16_UNORM";
		case FORMAT_R16G16_UNORM: return "R16G16_UNORM";
		case FORMAT_R16G16B16A16_UNORM: return "R16G16B16A16_UNORM";
		case FORMAT_R16G16B16A16_SFLOAT: return "R16G16B16A16_SFLOAT";
		case FORMAT_R32_UINT: return "R32_UINT";
		case FORMAT_R32_SFLOAT: return "R32_SFLOAT";
		case FORMAT_R32G32_SFLOAT: return "R32G32_SFLOAT";
		case FORMAT_R32G32B32_SFLOAT: return "R32G32B32_SFLOAT";
 		case FORMAT_R32G32B32A32_SFLOAT: return "R32G32B32A32_SFLOAT";
		case FORMAT_D32S8: return "D32S8";
		case FORMAT_R11G11B10_UFLOAT: return "R11G11B10_UFLOAT";
		case FORMAT_BC1: return "BC1";
		case FORMAT_BC3: return "BC3";
		case FORMAT_BC5: return "BC5";
		case FORMAT_BC7: return "BC7";
	}
	return "UNDEFINED";
}

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

struct RenderResource
{
	virtual void Release() = 0;
};

class RHISwapchain;
class RHIShader;
class RHIPipeline;
class RHIBuffer;
class RHITexture;
class RHIBindlessResources;
class RHIBottomLevelAccelerationStructure;
class RHITopLevelAccelerationStructure;

using RHISwapchainRef = Ref<RHISwapchain>;
using RHIShaderRef = Ref<RHIShader>;
using RHIPipelineRef = Ref<RHIPipeline>;
using RHIBufferRef = Ref<RHIBuffer>;
using RHITextureRef = Ref<RHITexture>;
using RHIBindlessResourcesRef = Ref<RHIBindlessResources>;
using RHIBottomLevelAccelerationStructureRef = Ref<RHIBottomLevelAccelerationStructure>;
using RHITopLevelAccelerationStructureRef = Ref<RHITopLevelAccelerationStructure>;
