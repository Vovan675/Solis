#pragma once
#include "RHI/RHIShader.h"
#include "VulkanUtils.h"

class VulkanDynamicRHI;

class VulkanShader final: public RHIShader
{
public:
	VulkanShader(const std::wstring &path, ShaderType type, std::wstring entry_point, std::vector<std::pair<const char *, const char *>> defines);
	~VulkanShader() { destroy(); }

	void destroy();
	void recompile() override;

	static std::vector<Descriptor> getDescriptors(std::vector<VulkanShader *> shaders);

	static std::vector<VkPushConstantRange> getPushConstantRanges(std::vector<Descriptor> &descriptors);

	VulkanDynamicRHI *rhi;

	VkShaderModule handle = nullptr;

	spvc_compiler compiler;
	spv_reflect::ShaderModule reflection;

	VkShaderModuleCreateInfo create_info;
};