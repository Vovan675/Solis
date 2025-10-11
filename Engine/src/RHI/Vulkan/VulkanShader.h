#pragma once
#include "RHI/RHIShader.h"
#include "VulkanUtils.h"

class VulkanDynamicRHI;

class VulkanShader final: public RHIShader
{
public:
	VulkanShader(const eastl::wstring &path, ShaderType type, eastl::wstring entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines);
	~VulkanShader() { destroy(); }

	void destroy();
	void recompile() override;

	static eastl::vector<Descriptor> getDescriptors(eastl::vector<VulkanShader *> shaders);

	static eastl::vector<VkPushConstantRange> getPushConstantRanges(eastl::vector<Descriptor> &descriptors);

	VulkanDynamicRHI *rhi;

	VkShaderModule handle = nullptr;

	spvc_compiler compiler;
	spv_reflect::ShaderModule reflection;

	VkShaderModuleCreateInfo create_info;

	eastl::hash_set<eastl::wstring> included_files;
};