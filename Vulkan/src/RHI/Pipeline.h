#pragma once
#include "Shader.h"
#include "EngineMath.h"
#include "Descriptors.h"
#include "Buffer.h"

using namespace Engine::Math;

struct PipelineDescription
{
	// Default Pipeline
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;
	bool use_vertices = true;
	DescriptorLayout descriptor_layout;
	std::vector<VkFormat> color_formats {};
	std::vector<VkPushConstantRange> push_constant_ranges {};
	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	bool use_depth_test = true;
	bool use_blending = true;
	VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
	VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
	VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;
	VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Compute Pipeline
	bool is_compute_pipeline = false;
	std::shared_ptr<Shader> compute_shader;
	
	// Ray Tracing Pipeline
	bool is_ray_tracing_pipeline = false;
	std::shared_ptr<Shader> ray_generation_shader;
	std::shared_ptr<Shader> miss_shader;
	std::shared_ptr<Shader> closest_hit_shader;

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
		hash_combine(hash, use_vertices);
		hash_combine(hash, descriptor_layout.hash);
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

	std::shared_ptr<Buffer> raygenShaderBindingTable;
	std::shared_ptr<Buffer> missShaderBindingTable;
	std::shared_ptr<Buffer> hitShaderBindingTable;
};

