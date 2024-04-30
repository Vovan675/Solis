#pragma once

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
	Shader(const VkDevice& device, const std::string& path, ShaderType type);
	virtual ~Shader();

	size_t getHash() const;
private:
	std::string read_file(const std::string& fileName) const;
	std::vector<uint32_t> compile_glsl(const std::string& glslSource, ShaderType type, const std::string debugName) const;
private:
	std::string path;
	std::string source;
	const VkDevice& m_DeviceHandle;
};

