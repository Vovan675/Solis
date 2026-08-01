#include "pch.h"
#include "AssetManager.h"
#include "ModelImporter.h"
#include "ModelImportSettings.h"
#include "RHI/RHITexture.h"
#include "Rendering/Model.h"
#include "Core/Filesystem.h"
#include "Core/Variables.h"

eastl::unordered_map<Engine::GUID, AssetMetadata> AssetManager::guid_to_metadata;
eastl::unordered_map<eastl::string, Ref<Asset>> AssetManager::cache_key_to_asset;
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

static eastl::string calc_cache_key(const AssetMetadata &metadata, const std::filesystem::path &path)
{
	if (metadata.isValid())
		return eastl::string("guid:") + eastl::to_string((uint64_t)metadata.asset_handle);
	return path.string().c_str();
}

static bool read_metadata_file(const std::filesystem::path &meta_path, const std::filesystem::path &source_path, AssetMetadata &out)
{
	YAML::Node node = YAML::LoadFile(meta_path.string());
	out.asset_handle = node["guid"].as<uint64_t>(0);
	out.runtime_handle = node["runtime_guid"].as<uint64_t>(0);
	out.type = (AssetType)node["type"].as<int>(ASSET_TYPE_UNDEFINED);
	out.params = node["Parameters"];
	out.source_path = source_path;
	return out.asset_handle.isValid();
}

template<class T, class Loader>
Ref<T> AssetManager::get_or_load(const eastl::string &path, Loader &&loader)
{
	const AssetMetadata &metadata = getOrCreateMetadata(path.c_str());
	eastl::string key = calc_cache_key(metadata, path.c_str());

	auto it = cache_key_to_asset.find(key);
	if (it != cache_key_to_asset.end())
		return it->second;

	if (metadata.isValid() && (engine_reimport_assets || !isRuntimeExists(path.c_str())))
		recreateRuntime(path.c_str());

	Ref<Asset> new_asset = loader();
	new_asset->asset_handle = metadata.asset_handle;
	cache_key_to_asset[key] = new_asset;
	return new_asset;
}

void AssetManager::init()
{
	index_assets(assets_root);
}

void AssetManager::shutdown()
{
	cache_key_to_asset.clear();
}

void AssetManager::index_assets(const std::filesystem::path &root)
{
	if (!std::filesystem::exists(root))
		return;

	for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it)
	{
		if (it->is_directory())
		{
			if (it->path().filename() == ".runtimes")
				it.disable_recursion_pending();
			continue;
		}
		if (it->path().extension() != ".meta")
			continue;

		std::filesystem::path source_path = it->path();
		source_path.replace_extension();

		AssetMetadata metadata;
		if (read_metadata_file(it->path(), source_path, metadata))
			register_metadata(metadata);
	}
}

AssetMetadata &AssetManager::register_metadata(const AssetMetadata &metadata)
{
	guid_to_metadata[metadata.asset_handle] = metadata;
	AssetMetadata &stored = guid_to_metadata[metadata.asset_handle];
	path_to_guid[Filesystem::canonicalPath(stored.source_path)] = stored.asset_handle;
	return stored;
}

RHITextureRef AssetManager::getTextureAsset(eastl::string path, TextureDescription desc)
{
	return get_or_load<RHITexture>(path, [&] { return load_texture_asset(path, desc); });
}

RHITextureRef AssetManager::getTextureAsset(eastl::string path)
{
	TextureDescription tex_description{};
	tex_description.format = FORMAT_R8G8B8A8_UNORM;
	tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;
	return getTextureAsset(path, tex_description);
}

RHITextureRef AssetManager::getTextureAssetByGuid(Engine::GUID guid, TextureDescription desc)
{
	std::filesystem::path path = getPathFromGUID(guid);
	if (path.empty())
		return nullptr;
	return getTextureAsset(path.string().c_str(), desc);
}

RHITextureRef AssetManager::getTextureAssetByGuid(Engine::GUID guid)
{
	std::filesystem::path path = getPathFromGUID(guid);
	if (path.empty())
		return nullptr;
	return getTextureAsset(path.string().c_str());
}

Ref<Model> AssetManager::getModelAsset(eastl::string path)
{
	auto std_path = std::filesystem::path(path.c_str());
	if (getAssetTypeFromExtension(std_path.extension().string().c_str()) != ASSET_TYPE_MODEL)
	{
		CORE_ERROR("Model extension not supported %s", path.c_str());
		return nullptr;
	}

	return get_or_load<Model>(path, [&] { return load_model_asset(path); });
}

Ref<Model> AssetManager::getModelAssetByGuid(Engine::GUID guid)
{
	std::filesystem::path path = getPathFromGUID(guid);
	if (path.empty())
		return nullptr;
	return getModelAsset(path.string().c_str());
}

const AssetMetadata &AssetManager::getOrCreateMetadata(const std::filesystem::path &source_path)
{
	const AssetMetadata &existing = getMetadata(source_path);
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
		AssetType asset_type = getAssetTypeFromExtension(source_path.extension().string().c_str());
		if (asset_type == ASSET_TYPE_UNDEFINED)
			return invalid_metadata;

		CORE_INFO("Importing {}", source_path.string());
		new_metadata.source_path = source_path;
		new_metadata.asset_handle = Engine::GUID();
		new_metadata.runtime_handle = Engine::GUID();
		new_metadata.type = asset_type;

		if (asset_type == ASSET_TYPE_TEXTURE)
		{
			new_metadata.params["generate_mipmaps"] = 1;
			new_metadata.params["format"] = "FORMAT_R8G8B8A8_SRGB";
		} else
		{
			ModelImportSettings defaults;
			defaults.saveToYAML(new_metadata.params);
		}
		saveMetadata(new_metadata);
	}

	return register_metadata(new_metadata);
}

void AssetManager::reimport(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;

	auto it = cache_key_to_asset.find(calc_cache_key(metadata, source_path));
	if (it == cache_key_to_asset.end())
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

	metadata.source_path = to;

	path_to_guid.erase(old_key);
	path_to_guid[Filesystem::canonicalPath(to)] = metadata.asset_handle;
}

void AssetManager::deleteAsset(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;

	Engine::GUID guid = metadata.asset_handle;
	eastl::string cache_key = calc_cache_key(metadata, source_path);
	eastl::string path_key = Filesystem::canonicalPath(source_path);
	std::filesystem::path runtime_path = getRuntimeAssetPath(source_path);
	std::filesystem::path meta_path = calc_meta_path(source_path);

	cache_key_to_asset.erase(cache_key);
	path_to_guid.erase(path_key);
	guid_to_metadata.erase(guid);

	std::filesystem::remove(source_path);
	std::filesystem::remove(meta_path);
	std::filesystem::remove(runtime_path);
}

std::filesystem::path AssetManager::getPathFromGUID(Engine::GUID guid)
{
	auto metadata = guid_to_metadata.find(guid);
	if (metadata != guid_to_metadata.end())
		return metadata->second.source_path;

	return std::filesystem::path();
}

Engine::GUID AssetManager::getGUIDFromPath(const std::filesystem::path &path)
{
	return getOrCreateMetadata(path).asset_handle;
}

AssetMetadata &AssetManager::getMetadata(const std::filesystem::path &source_path)
{
	auto it = path_to_guid.find(Filesystem::canonicalPath(source_path));
	if (it == path_to_guid.end())
		return invalid_metadata;

	auto meta = guid_to_metadata.find(it->second);
	if (meta == guid_to_metadata.end())
	{
		path_to_guid.erase(it);
		return invalid_metadata;
	}
	return meta->second;
}

void AssetManager::saveMetadata(const AssetMetadata &metadata)
{
	std::filesystem::path metadata_path = calc_meta_path(metadata.source_path);

	YAML::Node node;
	node["guid"] = (uint64_t)metadata.asset_handle;
	node["runtime_guid"] = (uint64_t)metadata.runtime_handle;
	node["type"] = (int)metadata.type;
	node["Parameters"] = metadata.params;

	std::ofstream file(metadata_path);
	YAML::Emitter out(file);
	out << node;
}

std::filesystem::path AssetManager::getRuntimeAssetPath(const std::filesystem::path &path)
{
	const AssetMetadata &metadata = getMetadata(path);
	if (metadata.isValid() && metadata.runtime_handle != 0)
		return compute_runtime_path(path, metadata.runtime_handle, get_runtime_extension(metadata.type));
	return std::filesystem::path();
}

bool AssetManager::isRuntimeExists(const std::filesystem::path &source_path)
{
	return std::filesystem::exists(getRuntimeAssetPath(source_path));
}

void AssetManager::recreateRuntime(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid() || metadata.type == ASSET_TYPE_UNDEFINED)
		return;

	CORE_INFO("Recreating Runtime for {}", source_path.string());

	std::filesystem::path runtime_path = getRuntimeAssetPath(source_path);
	std::filesystem::remove(runtime_path);

	if (metadata.type == ASSET_TYPE_TEXTURE)
	{
		int generate_mipmaps = metadata.params["generate_mipmaps"].as<int>(1);

		Ref<Image> image = new Image(source_path.string().c_str());

		if (generate_mipmaps)
			image->createMipmaps();

		std::filesystem::create_directories(runtime_path.parent_path());
		image->save(runtime_path);
	} else if (metadata.type == ASSET_TYPE_MODEL)
	{
		ModelImportSettings mesh_settings;
		mesh_settings.loadFromYAML(metadata.params);

		Ref<Model> model = new Model();
		ModelImporter::import(source_path.string().c_str(), model, mesh_settings);
	}
}

// Assets inside assets folder share one runtimes folder. External assets create runtimes folder near to them
std::filesystem::path AssetManager::compute_runtime_path(const std::filesystem::path &source_path, Engine::GUID runtime_guid, const eastl::string &extension)
{
	std::string root = assets_root.generic_string() + "/";
	std::string src = source_path.generic_string();
	bool is_in_assets = src.rfind(root, 0) == 0;
	std::filesystem::path runtimes = is_in_assets
		? assets_root / ".runtimes"
		: source_path.parent_path() / ".runtimes";
	eastl::string name = eastl::to_string(runtime_guid) + extension;
	return runtimes / name.c_str();
}

eastl::string AssetManager::get_runtime_extension(AssetType asset_type)
{
	switch (asset_type)
	{
		case ASSET_TYPE_TEXTURE: return ".dds";
		case ASSET_TYPE_MODEL: return ".mesh";
		default: return "";
	}
}

AssetType AssetManager::getAssetTypeFromExtension(eastl::string extension)
{
	extension.make_lower();
	static eastl::unordered_map<eastl::string, AssetType> extension_to_type =
	{
		{".dds", ASSET_TYPE_TEXTURE},
		{".png", ASSET_TYPE_TEXTURE},
		{".jpg", ASSET_TYPE_TEXTURE},
		{".jpeg", ASSET_TYPE_TEXTURE},
		{".fbx",  ASSET_TYPE_MODEL},
		{".obj",  ASSET_TYPE_MODEL},
		{".gltf", ASSET_TYPE_MODEL},
		{".glb",  ASSET_TYPE_MODEL},
	};

	auto it = extension_to_type.find(extension);
	return it == extension_to_type.end() ? ASSET_TYPE_UNDEFINED : it->second;
}

Ref<Asset> AssetManager::load_texture_asset(eastl::string path, TextureDescription desc)
{
	auto tex = gDynamicRHI->createTexture(desc);
	tex->load(path.c_str());
	return tex;
}

Ref<Asset> AssetManager::load_model_asset(eastl::string path)
{
	auto model = new Model();
	model->load(path.c_str());
	return model;
}
