#pragma once
#include "Shader.h"
#include "EngineMath.h"
#include "Descriptors.h"

using namespace Engine::Math;

struct PipelineDescription
{
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

	size_t getHash() const
	{
		size_t hash = 0;
		hash_combine(hash, vertex_shader->getHash());
		hash_combine(hash, fragment_shader->getHash());
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
private:
	size_t hash;
};

