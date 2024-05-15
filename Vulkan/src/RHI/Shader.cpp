#include "pch.h"
#include "Shader.h"
#include "Log.h"
#include "Math.h"
#include <shaderc/shaderc.hpp>
#include "VkWrapper.h"

Shader::Shader(const std::string& path, ShaderType type)
{
	this->type = type;
	this->path = path;
	this->source = read_file(path);
	std::vector<uint32_t> spirvBinary = compile_glsl(source, type, path);

	init_descriptors(spirvBinary);

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
	Engine::Math::hash_combine(hash, source);
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

void Shader::init_descriptors(const std::vector<uint32_t> &spirv)
{
	spvc_context context;
	spvc_context_create(&context);

	spvc_parsed_ir ir;
	spvc_context_parse_spirv(context, spirv.data(), spirv.size(), &ir);

	spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

	spvc_resources resources;
	spvc_compiler_create_shader_resources(compiler, &resources);

	const spvc_reflected_resource *list;
	size_t count;

	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &list, &count);
	parse_spv_resources(list, count, SPVC_RESOURCE_TYPE_PUSH_CONSTANT);

	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);
	parse_spv_resources(list, count, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER);

	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &list, &count);
	parse_spv_resources(list, count, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE);
}

void Shader::parse_spv_resources(const spvc_reflected_resource *resources, size_t count, spvc_resource_type resource_type)
{
	DescriptorStage stage;
	if (type == VERTEX_SHADER)
		stage = DESCRIPTOR_STAGE_VERTEX_SHADER;
	else if (type == FRAGMENT_SHADER)
		stage = DESCRIPTOR_STAGE_FRAGMENT_SHADER;

	DescriptorType descriptor_type;

	if (resource_type == SPVC_RESOURCE_TYPE_PUSH_CONSTANT)
		descriptor_type = DESCRIPTOR_TYPE_PUSH_CONSTANT;
	else if (resource_type == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER)
		descriptor_type = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	else if (resource_type == SPVC_RESOURCE_TYPE_SAMPLED_IMAGE)
		descriptor_type = DESCRIPTOR_TYPE_SAMPLER;

	for (size_t i = 0; i < count; i++)
	{
		unsigned int set = spvc_compiler_get_decoration(compiler, resources[i].id, SpvDecorationDescriptorSet);
		unsigned int binding = spvc_compiler_get_decoration(compiler, resources[i].id, SpvDecorationBinding);

		spvc_type struct_type = spvc_compiler_get_type_handle(compiler, resources[i].type_id);
		size_t size = 0;

		unsigned int first_member_offset = 0;
		if (resource_type == SPVC_RESOURCE_TYPE_PUSH_CONSTANT || resource_type == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER)
		{
			spvc_compiler_get_declared_struct_size(compiler, struct_type, &size);

			spvc_compiler_type_struct_member_offset(compiler, struct_type, 0, &first_member_offset);

			// because we need size of used structure, not full
			size -= first_member_offset;
		}

		const char *name = resources[i].name;
		auto &desc = descriptors.emplace_back(descriptor_type, set, binding, size, stage, name);
		desc.first_member_offset = first_member_offset;

		CORE_INFO("Shader: {} ID: {}, BaseTypeID: {}, TypeID: {}, Name: {}, Offset: {}\n", path, resources[i].id, resources[i].base_type_id, resources[i].type_id,
				  resources[i].name, first_member_offset);
		CORE_INFO("Set: {}, Binding: {}\n", set, binding);
	}
}
