#pragma once
#include "Core/Core.h"

struct AssetTypeInfo;
struct StructInfo;
class Asset;

// Firstly tries to resolve by guid, otherwise by path
struct AssetReference
{
	Engine::GUID guid = 0;
	eastl::string path;

	AssetReference() = default;
	explicit AssetReference(const std::filesystem::path &source_path);
	explicit AssetReference(const Asset *asset);

	void repair();

	bool isValid() const { return guid.isValid() || !path.empty(); }
	bool operator ==(const AssetReference &other) const { return guid == other.guid && path == other.path; }
};

class Asset : public RefCounted
{
public:
	Engine::GUID guid = 0;
	const AssetTypeInfo *type = nullptr;

	virtual void reload() {}
};

struct AssetMetadata
{
	Engine::GUID guid = 0;
	Engine::GUID runtimeGuid = 0;
	uint32_t runtimeVersion = 0;
	std::filesystem::path sourcePath;
	const AssetTypeInfo *type = nullptr;
	eastl::vector<uint8_t> importSettings;

	bool isValid() const
	{
		return guid.isValid() && type && !sourcePath.empty();
	}

	template<typename T>
	const T &getImportSettings() const { return *(const T *)importSettings.data(); }
};

struct AssetTypeInfo
{
	const char *name;
	eastl::vector<eastl::string> extensions;
	Ref<Asset> (*load)(const std::filesystem::path &path) = nullptr;
	const char *runtimeExtension = nullptr;
	void (*cook)(const AssetMetadata &metadata, const std::filesystem::path &runtime_path) = nullptr;
	const StructInfo *importSettingsInfo = nullptr;
	uint32_t (*runtimeVersion)(const AssetMetadata &metadata) = nullptr;

	const StructInfo *structInfo = nullptr;

	template<typename T>
	inline static const AssetTypeInfo *registered = nullptr;

	uint32_t getRuntimeVersion(const AssetMetadata &metadata) const { return runtimeVersion ? runtimeVersion(metadata) : 0; }
	bool isImported() const { return cook != nullptr; }
	bool isAuthored() const { return structInfo != nullptr; }
};
