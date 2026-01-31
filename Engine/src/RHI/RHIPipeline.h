#pragma once
#include "RHIShader.h"
#include "Math/EngineMath.h"
#include "RHIBuffer.h"
#include "Vulkan/Descriptors.h"
#include "DX12/DX12Shader.h"

using namespace Engine::Math;

enum CullMode
{
	CULL_MODE_NONE,
	CULL_MODE_FRONT,
	CULL_MODE_BACK
};

enum Topology
{
	TOPOLOGY_POINT_LIST,
	TOPOLOGY_LINE_LIST,
	TOPOLOGY_TRIANGLE_LIST,
	TOPOLOGY_TRIANGLE_STRIP
};

enum CompareFunc
{
	COMPARE_FUNC_NEVER,
	COMPARE_FUNC_LESS,
	COMPARE_FUNC_EQUAL,
	COMPARE_FUNC_LESS_EQUAL,
	COMPARE_FUNC_GREATER,
	COMPARE_FUNC_NOT_EQUAL,
	COMPARE_FUNC_GREATER_EQUAL,
	COMPARE_FUNC_ALWAYS
};

enum Blend
{
	BLEND_ZERO,
	BLEND_ONE,
	BLEND_SRC_COLOR,
	BLEND_ONE_MINUS_SRC_COLOR,
	BLEND_DST_COLOR,
	BLEND_ONE_MINUS_DST_COLOR,
	BLEND_SRC_ALPHA,
	BLEND_ONE_MINUS_SRC_ALPHA,
	BLEND_DST_ALPHA,
	BLEND_ONE_MINUS_DST_ALPHA,
	BLEND_SRC_ALPHA_SATURATE,
	BLEND_SRC1_COLOR,
	BLEND_ONE_MINUS_SRC1_COLOR,
	BLEND_SRC1_ALPHA,
	BLEND_ONE_MINUS_SRC1_ALPHA
};

enum BlendOp
{
	BLEND_OP_ADD,
	BLEND_OP_SUBTRACT,
	BLEND_OP_REV_SUBTRACT,
	BLEND_OP_MIN,
	BLEND_OP_MAX
};

enum class PipelineType
{
	Graphics,
	Compute,
	RayTracing
};

struct VertexInputsDescription
{
	struct VertexInput
	{
		const char *semantic_name;
		uint32_t vertex_buffer_slot;
		Format format;
		bool per_instance = false;
	};

	eastl::vector<VertexInput> inputs;

	size_t getHash() const
	{
		size_t hash = 0;
		for (const auto &input : inputs)
		{
			hash_combine(hash, input.vertex_buffer_slot);
			hash_combine(hash, input.format);
		}
		return hash;
	}
};

struct PipelineDescription
{
	// Default Pipeline
	RHIShaderRef vertex_shader;
	RHIShaderRef fragment_shader;

	VertexInputsDescription vertex_inputs_descriptions;
	eastl::vector<Format> color_formats {}; // MAX 8, TODO: replace with array
	eastl::vector<VkPushConstantRange> push_constant_ranges {};
	Format depth_format = FORMAT_UNDEFINED;
	bool use_depth_test = true;
	bool use_depth_write = true;
	CompareFunc depth_compare_func = COMPARE_FUNC_GREATER;
	bool use_blending = false;
	Blend src_color_blend = BLEND_SRC_ALPHA;
	Blend dst_color_blend = BLEND_ONE_MINUS_SRC_ALPHA;
	BlendOp color_blend_op = BLEND_OP_ADD;
	Blend src_alpha_blend = BLEND_ONE_MINUS_SRC_ALPHA;
	Blend dst_alpha_blend = BLEND_ONE_MINUS_SRC_ALPHA;
	BlendOp alpha_blend_op = BLEND_OP_ADD;
	CullMode cull_mode = CULL_MODE_BACK;
	Topology primitive_topology = TOPOLOGY_TRIANGLE_LIST;

	PipelineType pipeline_type = PipelineType::Graphics;

	// Compute Pipeline
	RHIShaderRef compute_shader;
	
	// Ray Tracing Pipeline
	RHIShaderRef ray_generation_shader;
	RHIShaderRef miss_shader;
	RHIShaderRef closest_hit_shader;

	size_t getHash() const
	{
		size_t hash = 0;
		if (vertex_shader)
			hash_combine(hash, vertex_shader->getHash());
		if (fragment_shader)
			hash_combine(hash, fragment_shader->getHash());
		if (compute_shader)
			hash_combine(hash, compute_shader->getHash());
		if (ray_generation_shader)
			hash_combine(hash, ray_generation_shader->getHash());
		if (miss_shader)
			hash_combine(hash, miss_shader->getHash());
		if (closest_hit_shader)
			hash_combine(hash, closest_hit_shader->getHash());

		hash_combine(hash, vertex_inputs_descriptions.getHash());
		for (const auto &color_format : color_formats)
			hash_combine(hash, color_format);

		for (const auto &range : push_constant_ranges)
		{
			hash_combine(hash, range.stageFlags);
			hash_combine(hash, range.offset);
			hash_combine(hash, range.size);
		}

		hash_combine(hash, depth_format);
		hash_combine(hash, use_depth_test);
		hash_combine(hash, use_depth_write);
		hash_combine(hash, depth_compare_func);
		hash_combine(hash, use_blending);
		hash_combine(hash, src_color_blend);
		hash_combine(hash, dst_color_blend);
		hash_combine(hash, color_blend_op);
		hash_combine(hash, src_alpha_blend);
		hash_combine(hash, dst_alpha_blend);
		hash_combine(hash, alpha_blend_op);
		hash_combine(hash, cull_mode);
		hash_combine(hash, primitive_topology);
		hash_combine(hash, (int)pipeline_type);
		return hash;
	}
};

class RHIPipeline : public RefCounted
{
public:
	virtual void create(const PipelineDescription &description) = 0;
	size_t getHash() const { return hash; };
protected:
	size_t hash = 0;
};
