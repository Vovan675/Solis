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
	RayTracing,
	Mesh
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
			hashCombine(hash, input.vertex_buffer_slot);
			hashCombine(hash, input.format);
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

	// Mesh Shader Pipeline
	RHIShaderRef mesh_shader;

	size_t getHash() const
	{
		size_t hash = 0;
		if (vertex_shader)
			hashCombine(hash, vertex_shader->getHash());
		if (fragment_shader)
			hashCombine(hash, fragment_shader->getHash());
		if (compute_shader)
			hashCombine(hash, compute_shader->getHash());
		if (ray_generation_shader)
			hashCombine(hash, ray_generation_shader->getHash());
		if (miss_shader)
			hashCombine(hash, miss_shader->getHash());
		if (closest_hit_shader)
			hashCombine(hash, closest_hit_shader->getHash());
		if (mesh_shader)
			hashCombine(hash, mesh_shader->getHash());

		hashCombine(hash, vertex_inputs_descriptions.getHash());
		for (const auto &color_format : color_formats)
			hashCombine(hash, color_format);

		for (const auto &range : push_constant_ranges)
		{
			hashCombine(hash, range.stageFlags);
			hashCombine(hash, range.offset);
			hashCombine(hash, range.size);
		}

		hashCombine(hash, depth_format);
		hashCombine(hash, use_depth_test);
		hashCombine(hash, use_depth_write);
		hashCombine(hash, depth_compare_func);
		hashCombine(hash, use_blending);
		hashCombine(hash, src_color_blend);
		hashCombine(hash, dst_color_blend);
		hashCombine(hash, color_blend_op);
		hashCombine(hash, src_alpha_blend);
		hashCombine(hash, dst_alpha_blend);
		hashCombine(hash, alpha_blend_op);
		hashCombine(hash, cull_mode);
		hashCombine(hash, primitive_topology);
		hashCombine(hash, (int)pipeline_type);
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
