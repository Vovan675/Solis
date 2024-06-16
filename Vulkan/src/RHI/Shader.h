#pragma once
#include <spirv_cross/spirv_cross_c.h>

enum DescriptorStage : uint32_t
{
	DESCRIPTOR_STAGE_VERTEX_SHADER = 1,
	DESCRIPTOR_STAGE_FRAGMENT_SHADER = 2,
};

enum DescriptorType
{
	DESCRIPTOR_TYPE_PUSH_CONSTANT,
	DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	DESCRIPTOR_TYPE_SAMPLER,
};

struct Descriptor
{
	Descriptor(DescriptorType type, unsigned int set, unsigned int binding, size_t size, DescriptorStage stage, std::string name = "")
		: type(type), set(set), binding(binding), size(size), stage(stage), name(name)
	{

	}

	DescriptorType type;
	unsigned int set;
	unsigned int binding;
	size_t size;
	DescriptorStage stage;
	std::string name;

	unsigned int first_member_offset = 0;
};

class Shader
{
public:
	virtual ~Shader();

	VkShaderModule handle = nullptr;
	enum ShaderType
	{
		VERTEX_SHADER,
		FRAGMENT_SHADER
	};

	size_t getHash() const;

	const std::vector<Descriptor> &getDescriptors() const { return descriptors; }

	void recompile();
	static void recompileAllShaders();

	static std::shared_ptr<Shader> create(const std::string &path, ShaderType type);
private:
	Shader(const std::string &path, ShaderType type);
private:
	static std::string read_file(const std::string& fileName);
	std::vector<uint32_t> compile_glsl(const std::string& glslSource, ShaderType type, const std::string debugName) const;
	void init_descriptors(const std::vector<uint32_t>& spirv);
	void parse_spv_resources(const spvc_reflected_resource *resources, size_t count, spvc_resource_type resource_type);
private:
	ShaderType type;
	std::string path;
	std::string source;

	std::vector<Descriptor> descriptors;

	size_t hash;
	static std::unordered_map<size_t, std::shared_ptr<Shader>> cached_shaders;

	spvc_compiler compiler;
};

