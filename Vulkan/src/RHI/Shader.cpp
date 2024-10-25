#include "pch.h"
#include "Shader.h"
#include "Log.h"
#include "Math.h"
#include <shaderc/shaderc.hpp>
#include <filesystem>
#include "VkWrapper.h"

std::unordered_map<size_t, std::shared_ptr<Shader>> Shader::cached_shaders;

static class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
{
	shaderc_include_result *GetInclude(const char *requestedSource, shaderc_include_type type, const char *requestingSource, size_t includeDepth) override
	{
		std::string msg = std::string(requestingSource);
		msg += std::to_string(type);
		msg += static_cast<char>(includeDepth);

		const std::string name = std::string(requestedSource);
		const std::string contents = ReadFile("shaders/" + name);

		auto container = new std::array<std::string, 2>;
		(*container)[0] = name;
		(*container)[1] = contents;

		auto data = new shaderc_include_result;

		data->user_data = container;

		data->source_name = (*container)[0].data();
		data->source_name_length = (*container)[0].size();

		data->content = (*container)[1].data();
		data->content_length = (*container)[1].size();

		return data;
	}
	void ReleaseInclude(shaderc_include_result *data) override
	{
		delete static_cast<std::array<std::string, 2> *>(data->user_data);
		delete data;
	}
	static std::string ReadFile(const std::string &filepath)
	{
		std::ifstream file(filepath, std::ios::binary | std::ios::ate);
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
};

Shader::Shader(const std::string& path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines)
{
	this->type = type;
	this->path = path;
	this->defines = defines;
	recompile();
}

Shader::~Shader()
{
	shutdown();
}

void Shader::shutdown()
{
	if (handle)
		vkDestroyShaderModule(VkWrapper::device->logicalHandle, handle, nullptr);
	handle = nullptr;
}

size_t Shader::getHash() const
{
	return hash;
}

void Shader::recompile()
{
	if (handle)
		vkDestroyShaderModule(VkWrapper::device->logicalHandle, handle, nullptr);

	this->source = read_file(path);
	std::vector<uint32_t> spirvBinary = compile_glsl(source, type, path);

	init_descriptors(spirvBinary);

	VkShaderModuleCreateInfo vertShaderModuleCreateInfo{};
	vertShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertShaderModuleCreateInfo.codeSize = spirvBinary.size() * 4; // Size must be in bytes, but vector of uint32
	vertShaderModuleCreateInfo.pCode = (const uint32_t *)spirvBinary.data();

	CHECK_ERROR(vkCreateShaderModule(VkWrapper::device->logicalHandle, &vertShaderModuleCreateInfo, nullptr, &handle));

	hash = 0;
	Engine::Math::hash_combine(hash, path);
	Engine::Math::hash_combine(hash, source);
	std::string all_defines = "";
	for (const auto &define : defines)
	{
		all_defines += define.first;
		all_defines += define.second;
	}
	Engine::Math::hash_combine(hash, all_defines);
}

void Shader::recompileAllShaders()
{
	for (auto& shader : cached_shaders)
	{
		shader.second->recompile();
	}
}

void Shader::destroyAllShaders()
{
	for (auto &shader : cached_shaders)
		shader.second->shutdown();
	cached_shaders.clear();
}

std::shared_ptr<Shader> Shader::create(const std::string &path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines)
{
	size_t hash = 0;
	Engine::Math::hash_combine(hash, path);

	std::string all_defines = "";
	for (const auto &define : defines)
	{
		all_defines += define.first;
		all_defines += define.second;
	}
	Engine::Math::hash_combine(hash, all_defines);

	// Try to find cached shader
	auto cached_shader = cached_shaders.find(hash);
	if (cached_shader != cached_shaders.end())
	{
		return cached_shader->second;
	}

	auto shader = std::shared_ptr<Shader>(new Shader(path, type, defines));
	cached_shaders[hash] = shader;
	return shader;
}

std::string Shader::read_file(const std::string& fileName)
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

	std::string start = "#version 460 \n"
		"#extension GL_GOOGLE_include_directive : enable \n"
		"#extension GL_EXT_nonuniform_qualifier : enable \n";

	return start + std::string(binary.data(), size);
}

std::vector<uint32_t> Shader::compile_glsl(const std::string& glslSource, ShaderType type, const std::string debugName) const
{
	// Create compiler
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetOptimizationLevel(shaderc_optimization_level_zero); // TODO: toggle
	//options.SetOptimizationLevel(shaderc_optimization_level_performance);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
	options.SetIncluder(std::make_unique<ShaderIncluder>());


	shaderc_shader_kind kind;
	switch (type)
	{
		case VERTEX_SHADER:
			kind = shaderc_vertex_shader;
			options.AddMacroDefinition("VERTEX_SHADER");
			break;
		case FRAGMENT_SHADER:
			kind = shaderc_fragment_shader;
			options.AddMacroDefinition("FRAGMENT_SHADER");
			break;
		case COMPUTE_SHADER:
			kind = shaderc_compute_shader;
			options.AddMacroDefinition("COMPUTE_SHADER");
			break;
		case RAY_GENERATION_SHADER:
			kind = shaderc_raygen_shader;
			options.AddMacroDefinition("RAY_GENERATION_SHADER");
			break;
		case MISS_SHADER:
			kind = shaderc_miss_shader;
			options.AddMacroDefinition("MISS_SHADER");
			break;
		case CLOSEST_HIT_SHADER:
			kind = shaderc_closesthit_shader;
			options.AddMacroDefinition("CLOSEST_HIT_SHADER");
			break;
	}

	for (const auto &define : defines)
	{
		options.AddMacroDefinition(define.first, define.second);
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
	descriptors.clear();

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

	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &list, &count);
	parse_spv_resources(list, count, SPVC_RESOURCE_TYPE_STORAGE_BUFFER);

	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &list, &count);
	parse_spv_resources(list, count, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE);

	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &list, &count);
	parse_spv_resources(list, count, SPVC_RESOURCE_TYPE_STORAGE_IMAGE);

	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE, &list, &count);
	parse_spv_resources(list, count, SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE);

	spvc_context_destroy(context);
}

void Shader::parse_spv_resources(const spvc_reflected_resource *resources, size_t count, spvc_resource_type resource_type)
{
	DescriptorStage stage;
	switch (type)
	{
		case VERTEX_SHADER:
			stage = DESCRIPTOR_STAGE_VERTEX_SHADER;
			break;
		case FRAGMENT_SHADER:
			stage = DESCRIPTOR_STAGE_FRAGMENT_SHADER;
			break;
		case COMPUTE_SHADER:
			stage = DESCRIPTOR_STAGE_COMPUTE_SHADER;
			break;
		case RAY_GENERATION_SHADER:
			stage = DESCRIPTOR_STAGE_RAY_GENERATION_SHADER;
			break;
		case MISS_SHADER:
			stage = DESCRIPTOR_STAGE_MISS_SHADER;
			break;
		case CLOSEST_HIT_SHADER:
			stage = DESCRIPTOR_STAGE_CLOSEST_HIT_SHADER;
			break;
	}

	DescriptorType descriptor_type;

	if (resource_type == SPVC_RESOURCE_TYPE_PUSH_CONSTANT)
		descriptor_type = DESCRIPTOR_TYPE_PUSH_CONSTANT;
	else if (resource_type == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER)
		descriptor_type = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	else if (resource_type == SPVC_RESOURCE_TYPE_STORAGE_BUFFER)
		descriptor_type = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	else if (resource_type == SPVC_RESOURCE_TYPE_SAMPLED_IMAGE)
		descriptor_type = DESCRIPTOR_TYPE_SAMPLER;
	else if (resource_type == SPVC_RESOURCE_TYPE_STORAGE_IMAGE)
		descriptor_type = DESCRIPTOR_TYPE_STORAGE_IMAGE;
	else if (resource_type == SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE)
		descriptor_type = DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE;

	for (size_t i = 0; i < count; i++)
	{
		unsigned int set = spvc_compiler_get_decoration(compiler, resources[i].id, SpvDecorationDescriptorSet);
		unsigned int binding = spvc_compiler_get_decoration(compiler, resources[i].id, SpvDecorationBinding);

		spvc_type struct_type = spvc_compiler_get_type_handle(compiler, resources[i].type_id);
		size_t size = 0;

		unsigned int first_member_offset = 0;
		if (resource_type == SPVC_RESOURCE_TYPE_PUSH_CONSTANT || resource_type == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER || resource_type == SPVC_RESOURCE_TYPE_STORAGE_BUFFER)
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
