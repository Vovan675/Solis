#include "pch.h"
#include "Shader.h"
#include "Log.h"
#include "MathUtils.h"
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross_c.h>
#include "VkWrapper.h"

Shader::Shader(const std::string& path, ShaderType type)
{
	this->path = path;
	this->source = read_file(path);
	std::vector<uint32_t> spirvBinary = compile_glsl(source, type, path);
	
	spvc_context context;
	spvc_context_create(&context);

	spvc_parsed_ir ir;
	spvc_context_parse_spirv(context, spirvBinary.data(), spirvBinary.size(), &ir);

	spvc_compiler compiler;
	spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

	spvc_resources resources;
	spvc_compiler_create_shader_resources(compiler, &resources);

	const spvc_reflected_resource *list;
	size_t count;
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &list, &count);

	for (size_t i = 0; i < count; i++)
	{
		unsigned int set = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet);
		unsigned int binding = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);

		spvc_type struct_type = spvc_compiler_get_type_handle(compiler, list[i].type_id);
		size_t size;
		spvc_compiler_get_declared_struct_size(compiler, struct_type, &size);

		unsigned int first_member_offset;
		spvc_compiler_type_struct_member_offset(compiler, struct_type, 0, &first_member_offset);

		// because we need size of used structure, not full
		size -= first_member_offset;

		DescriptorStage stage;
		if (type == VERTEX_SHADER)
			stage = DESCRIPTOR_STAGE_VERTEX_SHADER;
		else if (type == FRAGMENT_SHADER)
			stage = DESCRIPTOR_STAGE_FRAGMENT_SHADER;

		const char *name = list[i].name;
		auto &desc = descriptors.emplace_back(DESCRIPTOR_TYPE_PUSH_CONSTANT, set, binding, size, stage, name);
		desc.first_member_offset = first_member_offset;

		CORE_INFO("Shader: {} ID: {}, BaseTypeID: {}, TypeID: {}, Name: {}, Offset: {}\n", path, list[i].id, list[i].base_type_id, list[i].type_id,
				  list[i].name, first_member_offset);
		CORE_INFO("Set: {}, Binding: {}\n",
				  spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet),
				  spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding));
	}

	//spirv_cross::Compiler comp(spirvBinary);
	//spirv_cross::ShaderResources resources = comp.get_shader_resources();
	//spvReflectCreaateShaderModule
	//spirv_cross::CompilerReflection re(spirvBinary);
	VkShaderModuleCreateInfo vertShaderModuleCreateInfo{};
	vertShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertShaderModuleCreateInfo.codeSize = spirvBinary.size() * 4; // Size must be in bytes, but vector of uint32
	vertShaderModuleCreateInfo.pCode = (const uint32_t*)spirvBinary.data();

	CHECK_ERROR(vkCreateShaderModule(VkWrapper::device->logicalHandle, &vertShaderModuleCreateInfo, nullptr, &handle));
}

Shader::~Shader()
{
	vkDestroyShaderModule(VkWrapper::device->logicalHandle, handle, nullptr);
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
	options.SetOptimizationLevel(shaderc_optimization_level_zero);
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
