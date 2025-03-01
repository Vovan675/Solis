#pragma once
#include "RHIShader.h"
#include "EngineMath.h"
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

struct VertexInputsDescription
{
	struct VertexInput
	{
		const char *semantic_name;
		uint32_t semantic_index;
		Format format;
	};

	std::vector<VertexInput> inputs;

	size_t getHash() const
	{
		size_t hash = 0;
		for (const auto &input : inputs)
		{
			hash_combine(hash, input.semantic_index);
			hash_combine(hash, input.format);
		}
		return hash;
	}
};

struct PipelineDescription
{
	// Default Pipeline
	std::shared_ptr<RHIShader> vertex_shader;
	std::shared_ptr<RHIShader> fragment_shader;

	VertexInputsDescription vertex_inputs_descriptions;
	std::vector<Format> color_formats {}; // MAX 8, TODO: replace with array
	std::vector<VkPushConstantRange> push_constant_ranges {};
	Format depth_format = FORMAT_UNDEFINED;
	bool use_depth_test = true;
	bool use_blending = true;
	VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
	VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
	CullMode cull_mode = CULL_MODE_BACK;
	Topology primitive_topology = TOPOLOGY_TRIANGLE_LIST;

	// Compute Pipeline
	bool is_compute_pipeline = false;
	std::shared_ptr<RHIShader> compute_shader;
	
	// Ray Tracing Pipeline
	bool is_ray_tracing_pipeline = false;
	std::shared_ptr<RHIShader> ray_generation_shader;
	std::shared_ptr<RHIShader> miss_shader;
	std::shared_ptr<RHIShader> closest_hit_shader;

	size_t getHash() const
	{
		size_t hash = 0;
		if (vertex_shader)
			hash_combine(hash, vertex_shader->getHash());
		if (fragment_shader)
			hash_combine(hash, fragment_shader->getHash());
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
		hash_combine(hash, use_blending);
		hash_combine(hash, cull_mode);
		hash_combine(hash, primitive_topology);
		return hash;
	}
};

// TODO: remove when ray tracing done
/*
class Pipeline
{
public:
	virtual ~Pipeline();

	void cleanup();
	void create(const PipelineDescription &description);

	size_t getHash() const { return hash; };
public:
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipeline_layout = nullptr;
public:
	size_t hash;
	PipelineDescription description;

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

	std::shared_ptr<RHIBuffer> raygenShaderBindingTable;
	std::shared_ptr<RHIBuffer> missShaderBindingTable;
	std::shared_ptr<RHIBuffer> hitShaderBindingTable;
};
*/

class RHIPipeline : public GPUResource
{
public:
	virtual void create(const PipelineDescription &description) = 0;
	size_t getHash() const { return hash; };
protected:
	size_t hash = 0;
};

