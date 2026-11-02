#pragma once
#include "RHI/RHIShader.h"
#include "VulkanUtils.h"
#include <spirv_reflect.h>

class VulkanShader final: public RHIShader
{
public:
	VulkanShader(const eastl::wstring &path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines);
	~VulkanShader() { destroy(); }

	void destroy();

	static eastl::vector<Descriptor> getDescriptors(const eastl::fixed_vector<RHIShader *, 3> &shaders);

	static eastl::vector<VkPushConstantRange> getPushConstantRanges(eastl::vector<Descriptor> &descriptors);

	VkShaderModule handle = nullptr;

	spv_reflect::ShaderModule reflection;

private:
	void load_bytecode(IDxcBlob *bytecode) override;
};
