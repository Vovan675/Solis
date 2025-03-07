#pragma once
#include "RHI/RHIShader.h"
#include "VulkanUtils.h"

class VulkanDynamicRHI;

class VulkanShader : public RHIShader
{
public:
	VulkanShader(const uint32_t* spirvCode, size_t codeSize, VulkanDynamicRHI *rhi, ShaderType type, size_t hash);
	~VulkanShader() { destroy(); }

	void destroy();

	static std::vector<Descriptor> getDescriptors(std::vector<VulkanShader *> shaders);

	static std::vector<VkPushConstantRange> getPushConstantRanges(std::vector<Descriptor> &descriptors);

	ShaderType type;
	VulkanDynamicRHI *rhi;

	VkShaderModule handle;

	spvc_compiler compiler;
	spv_reflect::ShaderModule reflection;

	VkShaderModuleCreateInfo create_info;
};