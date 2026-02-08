#pragma once
#include <spirv_cross/spirv_cross_c.h>
#include "RHI/RHIDefinitions.h"
#include <spirv_reflect.h>

enum DescriptorStage : uint32_t
{
	// Default pipeline
	DESCRIPTOR_STAGE_VERTEX_SHADER = 2 << 0,
	DESCRIPTOR_STAGE_FRAGMENT_SHADER = 2 << 1,
	// Compute pipeline
	DESCRIPTOR_STAGE_COMPUTE_SHADER = 2 << 2,
	// Ray tracing pipeline
	DESCRIPTOR_STAGE_RAY_GENERATION_SHADER = 2 << 3,
	DESCRIPTOR_STAGE_MISS_SHADER = 2 << 4,
	DESCRIPTOR_STAGE_CLOSEST_HIT_SHADER = 2 << 5,
};

enum DescriptorType
{
	DESCRIPTOR_TYPE_PUSH_CONSTANT,
	DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	DESCRIPTOR_TYPE_STORAGE_BUFFER,
	DESCRIPTOR_TYPE_SAMPLER,
	DESCRIPTOR_TYPE_SAMPLED_IMAGE,
	DESCRIPTOR_TYPE_COMBINED_IMAGE,
	DESCRIPTOR_TYPE_STORAGE_IMAGE,
	DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE,
};

struct Descriptor
{
	Descriptor(DescriptorType type, unsigned int set, unsigned int binding, size_t size, DescriptorStage stage, eastl::string name = "")
		: type(type), set(set), binding(binding), size(size), stage(stage), name(name)
	{

	}

	DescriptorType type;
	unsigned int set;
	unsigned int binding;
	size_t size;
	DescriptorStage stage;
	eastl::string name;

	unsigned int first_member_offset = 0;
};

class RHIShader: public RefCounted
{
public:
	RHIShader(const eastl::wstring &path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines);
	virtual ~RHIShader();

	size_t getHash() const { return hash; }
	eastl::string getEntry() const { return entry_point.c_str(); }

	virtual void recompile() = 0;

	static eastl::list<RHIShader *> getAllShadersAtPath(const eastl::wstring &path);

protected:
	size_t hash = 0;
	std::filesystem::path path;
	ShaderType type;
	eastl::string entry_point;
	eastl::vector<eastl::pair<eastl::string, eastl::string>> defines;

	static eastl::unordered_map<eastl::wstring, eastl::list<RHIShader *>> path_to_shaders;
};
