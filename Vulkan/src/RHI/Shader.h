#pragma once

enum DescriptorStage : uint32_t
{
	DESCRIPTOR_STAGE_VERTEX_SHADER = 1,
	DESCRIPTOR_STAGE_FRAGMENT_SHADER = 2,
};

enum DescriptorType
{
	DESCRIPTOR_TYPE_PUSH_CONSTANT
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
	VkShaderModule handle;
	enum ShaderType
	{
		VERTEX_SHADER,
		FRAGMENT_SHADER
	};
public:
	Shader(const std::string& path, ShaderType type);
	virtual ~Shader();

	size_t getHash() const;

	const std::vector<Descriptor> &getDescriptors() const { return descriptors; }
private:
	std::string read_file(const std::string& fileName) const;
	std::vector<uint32_t> compile_glsl(const std::string& glslSource, ShaderType type, const std::string debugName) const;
private:
	std::string path;
	std::string source;

	std::vector<Descriptor> descriptors;
};

