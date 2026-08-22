#include "pch.h"
#include "AssetManager.h"
#include "RHI/RHITexture.h"
#include "Rendering/Model.h"
#include "Core/Filesystem.h"
#include "Core/Variables.h"

eastl::unordered_map<Engine::GUID, AssetMetadata> AssetManager::guid_to_metadata;
eastl::unordered_map<Engine::GUID, Ref<Asset>> AssetManager::guid_to_asset;
eastl::unordered_map<eastl::string, Engine::GUID> AssetManager::path_to_guid;
std::filesystem::path AssetManager::assets_root = "assets";
entt::sigh<void(Asset *)> AssetManager::pre_reimport_signal;
entt::sigh<void(Asset *)> AssetManager::post_reimport_signal;

static AssetMetadata invalid_metadata;

static std::filesystem::path calc_meta_path(const std::filesystem::path &source_path)
{
	std::filesystem::path path = source_path;
	return path.replace_extension(source_path.extension().string() + ".meta");
}

static bool read_metadata_file(const std::filesystem::path &meta_path, const std::filesystem::path &source_path, AssetMetadata &out)
{
	YAML::Node node = YAML::LoadFile(meta_path.string());
	out.guid = node["guid"].as<uint64_t>(0);
	out.runtimeGuid = node["runtime_guid"].as<uint64_t>(0);
	out.runtimeVersion = node["runtime_version"].as<uint32_t>(0);
	out.type = AssetManager::findTypeInfoByName(node["type"].as<std::string>("").c_str());
	out.sourcePath = source_path;
	if (!out.guid.isValid() || !out.type)
		return false;

	const StructInfo *info = out.type->importSettingsInfo;
	if (info)
	{
		const uint8_t *default_settings = (const uint8_t *)info->defaults;
		out.importSettings.assign(default_settings, default_settings + info->size);
		ReflectionYaml::readFields(node["Parameters"], *info, out.importSettings.data());
	}
	return true;
}

template<class T, class Loader>
Ref<T> AssetManager::get_or_load(const std::filesystem::path &path, Loader &&loader)
{
	const AssetMetadata &metadata = getOrCreateMetadata(path);
	if (!metadata.isValid())
		return nullptr;

	auto it = guid_to_asset.find(metadata.guid);
	if (it != guid_to_asset.end())
		return it->second;

	if (engine_assets_reimport || !hasValidRuntime(path))
		recreateRuntime(path);

	Ref<Asset> new_asset = loader();
	if (!new_asset)
		return nullptr;

	new_asset->guid = metadata.guid;
	new_asset->type = metadata.type;
	guid_to_asset[metadata.guid] = new_asset;
	return new_asset;
}

Ref<Asset> AssetManager::get_or_load(const std::filesystem::path &path, const AssetTypeInfo *type)
{
	return get_or_load<Asset>(path, [&] { return type->load(path); });
}

void AssetManager::init()
{
	refresh();
}

void AssetManager::refresh()
{
	if (!std::filesystem::exists(assets_root))
		return;

	eastl::vector<std::filesystem::path> junk_metas;

	for (auto it = std::filesystem::recursive_directory_iterator(assets_root); it != std::filesystem::recursive_directory_iterator(); ++it)
	{
		if (it->is_directory())
		{
			if (it->path().filename() == ".runtimes")
				it.disable_recursion_pending();
			continue;
		}

		if (it->path().extension() == ".meta")
		{
			std::filesystem::path source_path = it->path();
			source_path.replace_extension();
			if (!std::filesystem::exists(source_path))
				junk_metas.push_back(it->path());
			continue;
		}

		getOrCreateMetadata(it->path());
	}

	for (const std::filesystem::path &meta : junk_metas)
		std::filesystem::remove(meta);
}

void AssetManager::shutdown()
{
	guid_to_asset.clear();
}

Ref<Asset> AssetManager::getAsset(const std::filesystem::path &path, const AssetTypeInfo *type)
{
	if (path.empty() || !type)
		return nullptr;

	if (findTypeInfoByExtension(path.extension().string().c_str()) != type)
	{
		CORE_ERROR("{} is not a {} asset", path.string(), type->name);
		return nullptr;
	}

	return get_or_load(path, type);
}

RHITextureRef AssetManager::getTextureAsset(eastl::string path, TextureDescription desc)
{
	return get_or_load<RHITexture>(path.c_str(), [&]() -> Ref<Asset>
	{
		RHITextureRef texture = gDynamicRHI->createTexture(desc);
		texture->load(path.c_str());
		return texture;
	});
}

RHITextureRef AssetManager::getTextureAsset(const AssetReference &reference, TextureDescription desc)
{
	std::filesystem::path path = getPath(reference);
	if (path.empty())
		return nullptr;
	return getTextureAsset(path.string().c_str(), desc);
}

RHITextureRef AssetManager::getTextureAsset(eastl::string path) { return getAsset<RHITexture>(path.c_str()); }
Ref<Model> AssetManager::getModelAsset(eastl::string path) { return getAsset<Model>(path.c_str()); }

std::filesystem::path AssetManager::getPath(const AssetReference &reference)
{
	std::filesystem::path path = getPath(reference.guid);
	if (!path.empty())
		return path;
	return reference.path.empty() ? std::filesystem::path() : std::filesystem::path(reference.path.c_str());
}

void AssetManager::reimport(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;

	auto it = guid_to_asset.find(metadata.guid);
	if (it == guid_to_asset.end())
	{
		recreateRuntime(source_path);
		return;
	}

	pre_reimport_signal.publish(it->second);
	it->second->reload();
	post_reimport_signal.publish(it->second);
}

void AssetManager::moveAsset(const std::filesystem::path &from, const std::filesystem::path &to)
{
	AssetMetadata &metadata = getMetadata(from);
	if (!metadata.isValid())
		return;

	eastl::string old_key = Filesystem::canonicalPath(from);

	std::filesystem::rename(from, to);
	std::filesystem::rename(calc_meta_path(from), calc_meta_path(to));

	metadata.sourcePath = to;

	path_to_guid.erase(old_key);
	path_to_guid[Filesystem::canonicalPath(to)] = metadata.guid;
}

void AssetManager::deleteAsset(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;

	Engine::GUID guid = metadata.guid;
	eastl::string path_key = Filesystem::canonicalPath(source_path);
	std::filesystem::path runtime_path = calc_runtime_path(metadata);
	std::filesystem::path meta_path = calc_meta_path(source_path);

	guid_to_asset.erase(guid);
	path_to_guid.erase(path_key);
	guid_to_metadata.erase(guid);

	std::filesystem::remove(source_path);
	std::filesystem::remove(meta_path);
	std::filesystem::remove(runtime_path);
}

AssetMetadata &AssetManager::getOrCreateMetadata(const std::filesystem::path &source_path)
{
	AssetMetadata &existing = getMetadata(source_path);
	if (existing.isValid())
		return existing;

	if (!std::filesystem::exists(source_path))
		return invalid_metadata;

	AssetMetadata new_metadata;

	std::filesystem::path metadata_path = calc_meta_path(source_path);
	if (std::filesystem::exists(metadata_path))
	{
		if (!read_metadata_file(metadata_path, source_path, new_metadata))
			return invalid_metadata;
	} else
	{
		new_metadata.type = findTypeInfoByExtension(source_path.extension().string().c_str());
		if (!new_metadata.type)
			return invalid_metadata;

		new_metadata.sourcePath = source_path;
		new_metadata.guid = Engine::GUID::generate();

		if (new_metadata.type->isImported())
		{
			CORE_INFO("Importing {}", source_path.string());
			new_metadata.runtimeGuid = Engine::GUID::generate();
		}

		const StructInfo *info = new_metadata.type->importSettingsInfo;
		if (info)
		{
			const uint8_t *default_settings = (const uint8_t *)info->defaults;
			new_metadata.importSettings.assign(default_settings, default_settings + info->size);
		}
		saveMetadata(new_metadata);
	}

	AssetMetadata &stored = guid_to_metadata[new_metadata.guid];
	stored = new_metadata;
	path_to_guid[Filesystem::canonicalPath(stored.sourcePath)] = stored.guid;
	return stored;
}

AssetMetadata &AssetManager::getMetadata(const std::filesystem::path &source_path)
{
	auto it = path_to_guid.find(Filesystem::canonicalPath(source_path));
	if (it == path_to_guid.end())
		return invalid_metadata;

	return getMetadata(it->second);
}

AssetMetadata &AssetManager::getMetadata(Engine::GUID guid)
{
	auto it = guid_to_metadata.find(guid);
	return it != guid_to_metadata.end() ? it->second : invalid_metadata;
}

void AssetManager::saveMetadata(const AssetMetadata &metadata)
{
	std::ofstream file(calc_meta_path(metadata.sourcePath));
	YAML::Emitter out(file);

	out << YAML::BeginMap;
	out << YAML::Key << "guid" << YAML::Value << (uint64_t)metadata.guid;
	out << YAML::Key << "runtime_guid" << YAML::Value << (uint64_t)metadata.runtimeGuid;
	out << YAML::Key << "runtime_version" << YAML::Value << metadata.runtimeVersion;
	out << YAML::Key << "type" << YAML::Value << metadata.type->name;

	const StructInfo *info = metadata.type->importSettingsInfo;
	const void *settings = metadata.importSettings.data();
	if (info && !info->isDefault(settings, info->defaults))
	{
		out << YAML::Key << "Parameters" << YAML::Value << YAML::BeginMap;
		ReflectionYaml::writeFields(out, *info, settings, info->defaults);
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
}

std::filesystem::path AssetManager::calc_runtime_path(const AssetMetadata &metadata)
{
	if (!metadata.isValid() || !metadata.runtimeGuid.isValid())
		return std::filesystem::path();

	const std::filesystem::path &source_path = metadata.sourcePath;
	bool is_in_assets = source_path.generic_string().rfind(assets_root.generic_string() + "/", 0) == 0;
	std::filesystem::path runtimes = is_in_assets ? assets_root / ".runtimes" : source_path.parent_path() / ".runtimes";
	eastl::string name = eastl::to_string(metadata.runtimeGuid) + metadata.type->runtimeExtension;
	return runtimes / name.c_str();
}

std::filesystem::path AssetManager::getRuntimePath(const std::filesystem::path &source_path)
{
	return calc_runtime_path(getMetadata(source_path));
}

bool AssetManager::hasValidRuntime(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid() || metadata.runtimeVersion != metadata.type->getRuntimeVersion(metadata))
		return false;
	return std::filesystem::exists(calc_runtime_path(metadata));
}

void AssetManager::recreateRuntime(const std::filesystem::path &source_path)
{
	AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid() || !metadata.type->isImported() || !metadata.runtimeGuid.isValid())
		return;

	CORE_INFO("Recreating Runtime for {}", source_path.string());

	std::filesystem::path runtime_path = calc_runtime_path(metadata);
	std::filesystem::remove(runtime_path);
	metadata.type->cook(metadata, runtime_path);

	uint32_t runtime_version = metadata.type->getRuntimeVersion(metadata);
	if (metadata.runtimeVersion != runtime_version)
	{
		metadata.runtimeVersion = runtime_version;
		saveMetadata(metadata);
	}
}

const AssetTypeInfo *AssetManager::findTypeInfoByExtension(const eastl::string &extension)
{
	eastl::string lower = extension;
	lower.make_lower();
	for (const AssetTypeInfo *type : get_type_infos())
	{
		if (eastl::find(type->extensions.begin(), type->extensions.end(), lower) != type->extensions.end())
			return type;
	}
	return nullptr;
}

const AssetTypeInfo *AssetManager::findTypeInfoByName(const char *name)
{
	for (const AssetTypeInfo *type : get_type_infos())
	{
		if (strcmp(type->name, name) == 0)
			return type;
	}
	return nullptr;
}
