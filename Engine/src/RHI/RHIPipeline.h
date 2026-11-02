#pragma once
#include "RHIShader.h"
#include "Math/EngineMath.h"
#include "RHIBuffer.h"

using namespace Engine::Math;

enum class CullMode
{
	NONE,
	FRONT,
	BACK
};

enum class Topology
{
	POINT_LIST,
	LINE_LIST,
	TRIANGLE_LIST,
	TRIANGLE_STRIP
};

enum class CompareFunc
{
	NEVER,
	LESS,
	EQUAL,
	LESS_EQUAL,
	GREATER,
	NOT_EQUAL,
	GREATER_EQUAL,
	ALWAYS
};

enum class BlendFactor
{
	ZERO,
	ONE,
	SRC_COLOR,
	ONE_MINUS_SRC_COLOR,
	DST_COLOR,
	ONE_MINUS_DST_COLOR,
	SRC_ALPHA,
	ONE_MINUS_SRC_ALPHA,
	DST_ALPHA,
	ONE_MINUS_DST_ALPHA,
	SRC_ALPHA_SATURATE,
	SRC1_COLOR,
	ONE_MINUS_SRC1_COLOR,
	SRC1_ALPHA,
	ONE_MINUS_SRC1_ALPHA
};

enum class BlendOp
{
	ADD,
	SUBTRACT,
	REV_SUBTRACT,
	MIN,
	MAX
};

enum class PipelineType
{
	GRAPHICS,
	COMPUTE,
	RAY_TRACING,
	MESH
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
	Format depth_format = Format::UNDEFINED;
	bool use_depth_test = true;
	bool use_depth_write = true;
	CompareFunc depth_compare_func = CompareFunc::GREATER;
	bool use_blending = false;
	BlendFactor src_color_blend = BlendFactor::SRC_ALPHA;
	BlendFactor dst_color_blend = BlendFactor::ONE_MINUS_SRC_ALPHA;
	BlendOp color_blend_op = BlendOp::ADD;
	BlendFactor src_alpha_blend = BlendFactor::ONE_MINUS_SRC_ALPHA;
	BlendFactor dst_alpha_blend = BlendFactor::ONE_MINUS_SRC_ALPHA;
	BlendOp alpha_blend_op = BlendOp::ADD;
	CullMode cull_mode = CullMode::BACK;
	bool depth_clip = true;
	Topology primitive_topology = Topology::TRIANGLE_LIST;

	PipelineType pipeline_type = PipelineType::GRAPHICS;

	// Compute Pipeline
	RHIShaderRef compute_shader;
	
	// Ray Tracing Pipeline
	RHIShaderRef ray_generation_shader;
	RHIShaderRef miss_shader;
	RHIShaderRef closest_hit_shader;

	// Mesh Shader Pipeline
	RHIShaderRef mesh_shader;

	eastl::fixed_vector<RHIShader *, 3> getShaders() const
	{
		eastl::fixed_vector<RHIShader *, 3> shaders;
		switch (pipeline_type)
		{
			case PipelineType::GRAPHICS:
				shaders.push_back(vertex_shader);
				shaders.push_back(fragment_shader);
				break;
			case PipelineType::COMPUTE:
				shaders.push_back(compute_shader);
				break;
			case PipelineType::RAY_TRACING:
				shaders.push_back(ray_generation_shader);
				shaders.push_back(miss_shader);
				shaders.push_back(closest_hit_shader);
				break;
			case PipelineType::MESH:
				shaders.push_back(mesh_shader);
				shaders.push_back(fragment_shader);
				break;
		}
		return shaders;
	}

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
		hashCombine(hash, depth_clip);
		hashCombine(hash, primitive_topology);
		hashCombine(hash, (int)pipeline_type);
		return hash;
	}
};

class RHIPipeline : public RefCounted
{
public:
	RHIPipeline(const PipelineDescription &description): description(description), hash(description.getHash()) {}
	size_t getHash() const { return hash; }

	PipelineDescription description;

protected:
	size_t hash = 0;
};
