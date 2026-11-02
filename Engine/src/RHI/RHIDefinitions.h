#pragma once
#include "Core/Core.h"
#include "Math/EngineMath.h"
#include "Utils/Math.h"

#define ENABLE_RHI_VALIDATION

static const int MAX_FRAMES_IN_FLIGHT = 2;
static const int MAX_COLOR_ATTACHMENTS = 8;
static const uint32_t ALL_MIPS = UINT32_MAX;
static const uint32_t ALL_LAYERS = UINT32_MAX;

enum class ShaderType
{
	// Graphics pipeline
	VERTEX,
	FRAGMENT,
	// Compute pipeline
	COMPUTE,
	// Ray tracing pipeline
	RAY_GENERATION,
	MISS,
	CLOSEST_HIT,
	// Mesh Pipeline
	TASK,
	MESH,
};

enum class ResourceState
{
	UNDEFINED = 0,
	COMMON = 1 << 0,
	RENDER_TARGET = 1 << 1,
	DEPTH_READ = 1 << 2,
	SHADER_RESOURCE = 1 << 3,
	COPY_SRC = 1 << 4,
	COPY_DST = 1 << 5,
	UNORDERED_ACCESS = 1 << 6,
	PRESENT = 1 << 7,

	VERTEX_BUFFER = 1 << 8,
	INDEX_BUFFER = 1 << 9,
	INDIRECT_ARGS = 1 << 10,
};
ALLOW_ENUM_BITS(ResourceState)

enum class BufferUsage
{
	NONE = 0,
	VERTEX = 1 << 0,
	INDEX = 1 << 1,
	CONSTANT = 1 << 2,
	SHADER_RESOURCE = 1 << 3, // Read-only Storage Buffer (SRV)
	UNORDERED_ACCESS = 1 << 4, // Read/Write Storage Buffer (UAV)
	SCRATCH = 1 << 5, // Scratch Buffer for acceleration structures
	INDIRECT_ARGS = 1 << 6,
	ACCELERATION_STRUCTURE_BUILD_INPUT = 1 << 7,
	ACCELERATION_STRUCTURE_STORAGE = 1 << 8,
	SHADER_BINDING_TABLE = 1 << 9,
};
ALLOW_ENUM_BITS(BufferUsage)

enum class BufferViewType
{
	SHADER_RESOURCE,
	UNORDERED_ACCESS,
};

class RHIBuffer;
struct BufferViewDescription
{
	RHIBuffer *buffer;
	BufferViewType view_type;

	BufferViewDescription(RHIBuffer *buffer, BufferViewType view_type): buffer(buffer), view_type(view_type) {};
};

enum class IndexFormat
{
	UINT16,
	UINT32,
};

enum class BufferMemory
{
	GPU, // GPU only, use staging buffer for filling
	UPLOAD, // CPU writes, GPU reads
	READBACK, // GPU writes, CPU reads
};

struct BufferDescription
{
	uint64_t size = 0;
	BufferUsage usage_flags = BufferUsage::NONE;
	uint32_t alignment = 0;

	uint32_t stride = 4; // stride = 4 for raw access. Stride is more tied to data, thats why it is here instead of BufferView
	BufferMemory memory = BufferMemory::GPU;

	size_t getHash() const
	{
		size_t hash = 0;
		Engine::Math::hashCombine(hash, size);
		Engine::Math::hashCombine(hash, usage_flags);
		Engine::Math::hashCombine(hash, alignment);
		Engine::Math::hashCombine(hash, stride);
		Engine::Math::hashCombine(hash, memory);

		return hash;
	}
};

enum class Filter
{
	NEAREST,
	LINEAR
};

enum class SamplerAddressMode
{
	REPEAT,
	CLAMP_TO_EDGE,
	CLAMP_TO_BORDER
};

struct SamplerDescription
{
	Filter filter = Filter::LINEAR;
	SamplerAddressMode address_mode = SamplerAddressMode::REPEAT;
	bool anisotropy = false;
	bool use_comparison_less = false;
	float mip_bias = 0.0f;
};

enum class Format
{
	UNDEFINED = 0,

	// 8 bit
	R8_UNORM,
	R8G8_UNORM,
	R8G8B8A8_UNORM,
	R8G8B8A8_SRGB,

	// 16 bit
	R16_UNORM,
	R16_SFLOAT,
	R16G16_UNORM,
	R16G16_SFLOAT,
	R16G16_SINT,
	R16G16B16A16_UNORM,
	R16G16B16A16_SFLOAT,

	// 32 bit
	R32_UINT,
	R32_SFLOAT,
	R32G32_SFLOAT,
	R32G32B32_SFLOAT,
	R32G32B32A32_SFLOAT,

	// depth stencil
	D32_SFLOAT_S8_UINT,

	// combined
	R11G11B10_UFLOAT,

	// compressed
	BC1_UNORM,
	BC1_SRGB,
	BC3_UNORM,
	BC3_SRGB,
	BC5_UNORM,
	BC7_UNORM,
	BC7_SRGB,

	COUNT,
};

struct FormatInfo
{
	Format format;
	const char *name;
	uint32_t bytes_per_block;
	bool is_compressed;
	bool is_depth;
};

inline const FormatInfo &getFormatInfo(Format format)
{
	static const FormatInfo format_infos[] = {
		// format						name					bytes_per_block	is_compressed	is_depth
		{Format::UNDEFINED,				"UNDEFINED",			0,				false,			false},

		// 8 bit
		{Format::R8_UNORM,				"R8_UNORM",				1,				false,			false},
		{Format::R8G8_UNORM,			"R8G8_UNORM",			2,				false,			false},
		{Format::R8G8B8A8_UNORM,		"R8G8B8A8_UNORM",		4,				false,			false},
		{Format::R8G8B8A8_SRGB,			"R8G8B8A8_SRGB",		4,				false,			false},

		// 16 bit
		{Format::R16_UNORM,				"R16_UNORM",			2,				false,			false},
		{Format::R16_SFLOAT,			"R16_SFLOAT",			2,				false,			false},
		{Format::R16G16_UNORM,			"R16G16_UNORM",			4,				false,			false},
		{Format::R16G16_SFLOAT,			"R16G16_SFLOAT",		4,				false,			false},
		{Format::R16G16_SINT,			"R16G16_SINT",			4,				false,			false},
		{Format::R16G16B16A16_UNORM,	"R16G16B16A16_UNORM",	8,				false,			false},
		{Format::R16G16B16A16_SFLOAT,	"R16G16B16A16_SFLOAT",	8,				false,			false},

		// 32 bit
		{Format::R32_UINT,				"R32_UINT",				4,				false,			false},
		{Format::R32_SFLOAT,			"R32_SFLOAT",			4,				false,			false},
		{Format::R32G32_SFLOAT,			"R32G32_SFLOAT",		8,				false,			false},
		{Format::R32G32B32_SFLOAT,		"R32G32B32_SFLOAT",		12,				false,			false},
		{Format::R32G32B32A32_SFLOAT,	"R32G32B32A32_SFLOAT",	16,				false,			false},

		// depth stencil
		{Format::D32_SFLOAT_S8_UINT,	"D32_SFLOAT_S8_UINT",	8,				false,			true},

		// combined
		{Format::R11G11B10_UFLOAT,		"R11G11B10_UFLOAT",		4,				false,			false},

		// compressed
		{Format::BC1_UNORM,				"BC1_UNORM",			8,				true,			false},
		{Format::BC1_SRGB,				"BC1_SRGB",				8,				true,			false},
		{Format::BC3_UNORM,				"BC3_UNORM",			16,				true,			false},
		{Format::BC3_SRGB,				"BC3_SRGB",				16,				true,			false},
		{Format::BC5_UNORM,				"BC5_UNORM",			16,				true,			false},
		{Format::BC7_UNORM,				"BC7_UNORM",			16,				true,			false},
		{Format::BC7_SRGB,				"BC7_SRGB",				16,				true,			false},
	};
	static_assert(_countof(format_infos) == (size_t)Format::COUNT);
	return format_infos[(int)format];
}

inline bool isCompressedFormat(Format format) { return getFormatInfo(format).is_compressed; }
inline bool isDepthFormat(Format format) { return getFormatInfo(format).is_depth; }

inline uint32_t getFormatRowSize(Format format, uint32_t width)
{
	uint32_t columns = isCompressedFormat(format) ? Math::divideRoundUp(width, 4) : width;
	return columns * getFormatInfo(format).bytes_per_block;
}

inline uint32_t getFormatSliceSize(Format format, uint32_t width, uint32_t height)
{
	uint32_t rows = isCompressedFormat(format) ? Math::divideRoundUp(height, 4) : height;
	return rows * getFormatRowSize(format, width);
}

enum class TextureUsage : uint32_t
{
	NONE = 0,
	NO_SAMPLED = 1 << 0,
	UNORDERED_ACCESS = 1 << 1,
	RENDER_TARGET = 1 << 2,
};
ALLOW_ENUM_BITS(TextureUsage)

enum class TextureDimension
{
	TEXTURE_2D,
	TEXTURE_3D,
	TEXTURE_CUBE,
};

struct TextureDescription
{
	TextureDimension dimension = TextureDimension::TEXTURE_2D;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t depth = 1;
	uint32_t mip_levels = 1;
	uint32_t array_layers = 1;
	uint32_t sample_count = 1;
	Format format = Format::UNDEFINED;
	TextureUsage usage_flags = TextureUsage::NONE;
	float depth_clear_value = 0.0f; // 0.0 for reverse-Z (GBuffer), 1.0 for standard Z (shadows)

	size_t getHash() const
	{
		size_t hash = 0;
		Engine::Math::hashCombine(hash, dimension);
		Engine::Math::hashCombine(hash, width);
		Engine::Math::hashCombine(hash, height);
		Engine::Math::hashCombine(hash, depth);
		Engine::Math::hashCombine(hash, mip_levels);
		Engine::Math::hashCombine(hash, array_layers);
		Engine::Math::hashCombine(hash, sample_count);
		Engine::Math::hashCombine(hash, format);
		Engine::Math::hashCombine(hash, usage_flags);
		Engine::Math::hashCombine(hash, depth_clear_value);

		return hash;
	}
};

enum class TextureViewType
{
	RENDER_TARGET, 
	SHADER_RESOURCE,
	UNORDERED_ACCESS,
};

class RHITexture;
struct TextureViewDescription
{
	RHITexture *texture;
	TextureViewType view_type;
	uint32_t mip = ALL_MIPS;
	uint32_t layer = ALL_LAYERS;

	TextureViewDescription(RHITexture *texture, TextureViewType view_type): texture(texture), view_type(view_type) {};
	TextureViewDescription(RHITexture *texture, TextureViewType view_type, uint32_t mip, uint32_t layer): texture(texture), view_type(view_type), mip(mip), layer(layer) {};
};

struct RenderResource
{
	virtual void release() = 0;
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
using RHIBottomLevelAccelerationStructureRef = Ref<RHIBottomLevelAccelerationStructure>;
using RHITopLevelAccelerationStructureRef = Ref<RHITopLevelAccelerationStructure>;
