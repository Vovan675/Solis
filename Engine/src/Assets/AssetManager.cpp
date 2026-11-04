#include "pch.h"
#include "AssetManager.h"
#include "RHI/RHITexture.h"
#include "Rendering/Model.h"
#include "Core/Filesystem.h"
#include "Core/Variables.h"
#include "Math/EngineMath.h"
#include "Utils/BinaryArchive.h"
#include "Utils/FileMemory.h"

eastl::unordered_map<Engine::GUID, AssetManager::AssetRecord> AssetManager::guid_to_record;
eastl::unordered_map<eastl::string, Engine::GUID> AssetManager::path_to_guid;
std::filesystem::path AssetManager::assets_root = "assets";
size_t AssetManager::cache_version = 0;
bool AssetManager::cache_dirty = false;
uint32_t AssetManager::refresh_index = 0;
uint32_t AssetManager::generation = 0;
entt::sigh<void(Asset *)> AssetManager::pre_reimport_signal;
entt::sigh<void(Asset *)> AssetManager::post_reimport_signal;

static AssetMetadata invalid_metadata;

static const char *CACHE_PATH = "cache/assets_metadata.cache";
static const uint32_t CACHE_FORMAT_VERSION = 1;

static std::filesystem::path calc_meta_path(const std::filesystem::path &source_path)
{
	std::filesystem::path path = source_path;
	return path.replace_extension(source_path.extension().string() + ".meta");
}

static size_t calc_asset_record_hash(const std::filesystem::path &source_path)
{
	auto hash_file = [](const std::filesystem::path &path)
	{
		FileMemory file;
		if (!file.open(path.string().c_str(), true, 0))
			return 0ull;
		return Engine::Math::fnv1aHash(file.getData(), file.getSize());
	};

	size_t hash = hash_file(source_path);
	Engine::Math::hashCombine(hash, hash_file(calc_meta_path(source_path)));
	return hash;
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

void AssetManager::init()
{
	load_cache();
	refresh();
}

void AssetManager::refresh()
{
	if (!std::filesystem::exists(assets_root))
		return;

	// Scan: Collect all meta writes and sources
	eastl::vector<std::filesystem::directory_entry> source_files;
	std::unordered_map<std::string, std::filesystem::file_time_type> meta_write_times;
	for (auto it = std::filesystem::recursive_directory_iterator(assets_root); it != std::filesystem::recursive_directory_iterator(); ++it)
	{
		if (it->is_directory())
		{
			if (it->path().filename() == ".runtimes")
				it.disable_recursion_pending();
		} else if (it->path().extension() == ".meta")
		{
			meta_write_times[it->path().generic_string()] = it->last_write_time();
		} else
		{
			source_files.push_back(*it);
		}
	}

	// Check each source's meta
	refresh_index++;
	for (const std::filesystem::directory_entry &source_file : source_files)
	{
		const std::filesystem::path &source_path = source_file.path();
		const AssetMetadata &metadata = getOrCreateMetadata(source_path);
		if (!metadata.isValid())
			continue;

		AssetRecord &record = guid_to_record[metadata.guid];
		record.last_seen_refresh = refresh_index;

		// No meta in scan stage
		std::filesystem::path meta_path = calc_meta_path(source_path);
		auto meta = meta_write_times.find(meta_path.generic_string());
		if (meta == meta_write_times.end())
		{
			// If metadata just got from cache inside getOrCreateMetadata and file is deleted, then restore it back to disk
			if (!std::filesystem::exists(meta_path))
				saveMetadata(metadata);
			// In any case, we have valid metadata, dont make anything with it
			continue;
		}

		std::filesystem::file_time_type meta_write_time = meta->second;
		meta_write_times.erase(meta); // Consume, so in future can check which metas can be deleted

		// If source and meta files didn't touch, dont do anything
		std::filesystem::file_time_type source_write_time = source_file.last_write_time();
		if (record.source_write_time == source_write_time && record.meta_write_time == meta_write_time)
			continue;

		record.source_write_time = source_write_time;
		record.meta_write_time = meta_write_time;
		cache_dirty = true;

		// Touched, but real content is the same (just resaved)
		if (calc_asset_record_hash(source_path) == record.hash)
			continue;

		// Guid changed or invalid meta
		AssetMetadata new_metadata;
		if (!read_metadata_file(meta_path, source_path, new_metadata) || new_metadata.guid != metadata.guid)
		{
			// Remove asset with old guid
			remove_asset(metadata.guid);
			// Recreate asset with new guid and meta
			getOrCreateMetadata(source_path);
			continue;
		}

		// Metadata changed, and reimport with new metadata
		record.metadata = new_metadata;
		reimport(source_path);
	}

	// Removed: not seen in this scan and gone from disk (assets outside assets/ are never scanned)
	eastl::vector<Engine::GUID> removed_guids;
	for (const auto &[guid, record] : guid_to_record)
	{
		if (record.last_seen_refresh != refresh_index && !std::filesystem::exists(record.metadata.sourcePath))
			removed_guids.push_back(guid);
	}
	for (Engine::GUID guid : removed_guids)
		remove_asset(guid);

	// Meta that not found in any source just removed
	for (const auto &[meta_path, meta_write_time] : meta_write_times)
		std::filesystem::remove(meta_path);

	save_dirty_cache();
}

void AssetManager::shutdown()
{
	save_dirty_cache();
	guid_to_record.clear();
}

void AssetManager::remove_asset(Engine::GUID guid)
{
	auto it = guid_to_record.find(guid);
	Ref<Asset> asset = it->second.asset;
	path_to_guid.erase(Filesystem::canonicalPath(it->second.metadata.sourcePath));
	guid_to_record.erase(it);
	cache_dirty = true;
	generation++;

	if (!asset)
		return;

	pre_reimport_signal.publish(asset);
	post_reimport_signal.publish(asset);
}

void AssetManager::load_cache()
{
	cache_version = CACHE_FORMAT_VERSION;

	// Cache version also auto change with changing anything in asset types
	for (const AssetTypeInfo *type : get_type_infos())
	{
		const StructInfo *info = type->importSettingsInfo;
		if (!info)
			continue;

		for (int i = 0; i < info->fieldsCount; i++)
		{
			Engine::Math::hashCombine(cache_version, info->fields[i].name);
			Engine::Math::hashCombine(cache_version, info->fields[i].offset);
		}
	}

	FileMemory file;
	if (!file.open(CACHE_PATH, true, 0))
		return;

	BinaryArchive archive = BinaryArchive::createForLoading((const uint8_t *)file.getData(), file.getSize(), 0);
	size_t version = 0;
	archive << version;
	if (version != cache_version)
		return;

	uint32_t count = 0;
	archive << count;

	for (int i = 0; i < count; i++)
	{
		AssetRecord record;
		record.serialize(archive);
		if (!record.metadata.isValid())
			continue;

		path_to_guid[Filesystem::canonicalPath(record.metadata.sourcePath)] = record.metadata.guid;
		guid_to_record[record.metadata.guid] = record;
	}
}

void AssetManager::save_dirty_cache()
{
	if (!cache_dirty)
		return;
	cache_dirty = false;

	std::filesystem::create_directories("cache");
	std::ofstream file(CACHE_PATH, std::ios::binary);
	BinaryArchive archive = BinaryArchive::createForSaving(file);
	uint32_t count = guid_to_record.size();
	archive << cache_version;
	archive << count;
	for (auto &it : guid_to_record)
		it.second.serialize(archive);
}

void AssetManager::AssetRecord::serialize(BinaryArchive &archive)
{
	eastl::string type_name = metadata.type ? metadata.type->name : "";
	archive << metadata.guid << metadata.runtimeGuid << metadata.runtimeVersion << metadata.sourcePath << type_name << metadata.importSettings;
	archive << source_write_time << meta_write_time << hash;

	if (archive.isLoading())
		metadata.type = findTypeInfoByName(type_name.c_str());
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

	const AssetMetadata &metadata = getOrCreateMetadata(path);
	if (!metadata.isValid())
		return nullptr;

	AssetRecord &record = guid_to_record[metadata.guid];
	if (record.asset)
		return record.asset;

	if (engine_assets_reimport || !hasValidRuntime(path))
		recreateRuntime(path);

	Ref<Asset> new_asset = type->load(path);
	if (!new_asset)
		return nullptr;

	new_asset->guid = metadata.guid;
	new_asset->type = metadata.type;
	record.asset = new_asset;
	return new_asset;
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

	Ref<Asset> asset = guid_to_record[metadata.guid].asset;
	if (!asset)
	{
		std::filesystem::remove(calc_runtime_path(metadata));
		return;
	}

	pre_reimport_signal.publish(asset);
	asset->reload();
	post_reimport_signal.publish(asset);
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
	cache_dirty = true;

	path_to_guid.erase(old_key);
	path_to_guid[Filesystem::canonicalPath(to)] = metadata.guid;
}

void AssetManager::deleteAsset(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;

	std::filesystem::remove(source_path);
	std::filesystem::remove(calc_meta_path(source_path));
	std::filesystem::remove(calc_runtime_path(metadata));
	remove_asset(metadata.guid);
}

AssetMetadata &AssetManager::getOrCreateMetadata(const std::filesystem::path &source_path)
{
	AssetMetadata &existing = getMetadata(source_path);
	if (existing.isValid())
		return existing;

	std::error_code error;
	std::filesystem::file_time_type source_write_time = std::filesystem::last_write_time(source_path, error);
	if (error)
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

	AssetRecord &record = guid_to_record[new_metadata.guid];
	if (record.metadata.isValid())
		path_to_guid.erase(Filesystem::canonicalPath(record.metadata.sourcePath));
	record.metadata = new_metadata;
	record.source_write_time = source_write_time;
	record.meta_write_time = std::filesystem::last_write_time(metadata_path);
	path_to_guid[Filesystem::canonicalPath(source_path)] = new_metadata.guid;
	cache_dirty = true;
	generation++;
	return record.metadata;
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
	auto it = guid_to_record.find(guid);
	return it != guid_to_record.end() ? it->second.metadata : invalid_metadata;
}

void AssetManager::saveMetadata(const AssetMetadata &metadata)
{
	std::filesystem::path meta_path = calc_meta_path(metadata.sourcePath);
	std::ofstream file(meta_path);
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

	file.close();
	auto it = guid_to_record.find(metadata.guid);
	if (it != guid_to_record.end())
	{
		it->second.meta_write_time = std::filesystem::last_write_time(meta_path);
		cache_dirty = true;
	}
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

	guid_to_record[metadata.guid].hash = calc_asset_record_hash(source_path);
	cache_dirty = true;
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
