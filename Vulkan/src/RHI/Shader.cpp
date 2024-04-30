#include "pch.h"
#include "Shader.h"
#include "Log.h"
#include <shaderc/shaderc.hpp>
#include "MathUtils.h"

Shader::Shader(const VkDevice& device, const std::string& path, ShaderType type): m_DeviceHandle(device)
{
	this->path = path;
	this->source = read_file(path);
	std::vector<uint32_t> spirvBinary = compile_glsl(source, type, path);

	VkShaderModuleCreateInfo vertShaderModuleCreateInfo{};
	vertShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertShaderModuleCreateInfo.codeSize = spirvBinary.size() * 4; // Size must be in bytes, but vector of uint32
	vertShaderModuleCreateInfo.pCode = (const uint32_t*)spirvBinary.data();

	CHECK_ERROR(vkCreateShaderModule(device, &vertShaderModuleCreateInfo, nullptr, &handle));
}

Shader::~Shader()
{
	vkDestroyShaderModule(m_DeviceHandle, handle, nullptr);
}

size_t Shader::getHash() const
{
	size_t hash = 0;
	hash_combine(hash, source);
	return hash;
}

std::string Shader::read_file(const std::string& fileName) const
{
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		CORE_ERROR("Error opening file");
	}
	int size = file.tellg();
	file.seekg(0);//Back to start
	std::vector<char> binary(size);
	file.read(binary.data(), size);
	file.close();
	return std::string(binary.data(), size);
}

std::vector<uint32_t> Shader::compile_glsl(const std::string& glslSource, ShaderType type, const std::string debugName) const
{
	// Create compiler
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetOptimizationLevel(shaderc_optimization_level_size);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
	
	shaderc_shader_kind kind;
	if (type == FRAGMENT_SHADER)
	{
		kind = shaderc_fragment_shader;
	}
	else
	{
		kind = shaderc_vertex_shader;
	}
	
	// Preprocess source
	auto pp = compiler.PreprocessGlsl(glslSource, kind, "test.spv", options);
	std::string pptext(pp.cbegin(), pp.cend());

	// Compile source
	auto result = compiler.CompileGlslToSpv(pptext, kind, "test.spv", options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		CORE_CRITICAL(result.GetErrorMessage());
	}

	return std::vector<uint32_t>(result.cbegin(), result.cend());
}
