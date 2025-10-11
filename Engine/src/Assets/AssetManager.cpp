#include "pch.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "RHI/RHITexture.h"
#include "Rendering/Model.h"

eastl::unordered_map<Engine::GUID, AssetMetadata> AssetManager::registered_metadata;
eastl::unordered_map<eastl::string, Ref<Asset>> AssetManager::loaded_assets;

static AssetMetadata invalid_metadata;

void AssetManager::init()
{
	reloadAssets("assets/");
}

void AssetManager::shutdown()
{
	loaded_assets.clear();
}

RHITextureRef AssetManager::getTextureAsset(eastl::string path)
{
	TextureDescription tex_description{};
	tex_description.format = FORMAT_R8G8B8A8_UNORM;
	tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;
	return getTextureAsset(path, tex_description);
}

RHITextureRef AssetManager::getTextureAsset(eastl::string path, TextureDescription desc)
{
	if (loaded_assets.find(path) != loaded_assets.end())
		return loaded_assets[path];

	Ref<Asset> new_asset;

	auto std_path = std::filesystem::path(path.c_str());
	std::string extension = std_path.extension().string();
	new_asset = load_texture_asset(path, desc);

	loaded_assets[path] = new_asset;
	return new_asset;
}

Ref<Model> AssetManager::getModelAsset(eastl::string path)
{
	if (loaded_assets.find(path) != loaded_assets.end())
		return loaded_assets[path];

	Ref<Asset> new_asset;

	auto std_path = std::filesystem::path(path.c_str());
	std::string extension = std_path.extension().string();
	if (Assimp::Importer().IsExtensionSupported(extension))
	{
		new_asset = load_model_asset(path);
	} else
	{
		CORE_ERROR("Model extension not supported %s", path.c_str());
		return nullptr;
	}

	loaded_assets[path] = new_asset;
	return new_asset;
}

std::filesystem::path AssetManager::getPathFromGUID(Engine::GUID guid)
{
	auto metadata = registered_metadata.find(guid);
	if (metadata != registered_metadata.end())
		return metadata->second.source_path;

	return std::filesystem::path();
}

Engine::GUID AssetManager::getGUIDFromPath(const std::filesystem::path &path)
{
	const AssetMetadata &metadata = getMetadata(path);
	if (metadata.isValid())
		return metadata.asset_handle;
	else
		return importAsset(path).asset_handle;

	return 0;
}

std::filesystem::path AssetManager::getRuntimeAssetPath(const std::filesystem::path &path)
{
	const AssetMetadata &metadata = getMetadata(path);
	if (metadata.isValid() && metadata.runtime_handle != 0)
	{
		return getRuntimeAssetPath(metadata.runtime_handle, getRuntimeExtension(metadata.type));
	}

	return std::filesystem::path();
}

std::filesystem::path AssetManager::getRuntimeAssetPath(Engine::GUID runtime_guid, eastl::string extension)
{
	std::filesystem::path runtime_path = "assets/.runtimes/";
	eastl::string name = eastl::to_string(runtime_guid) + extension;
	runtime_path += name.c_str();
	return runtime_path;
}

eastl::string AssetManager::getRuntimeExtension(AssetType asset_type)
{
	static eastl::unordered_map<AssetType, eastl::string> type_to_extension =
	{
		{ASSET_TYPE_TEXTURE, ".dds"},
		{ASSET_TYPE_MODEL, ".mesh"},
	};

	if (type_to_extension.find(asset_type) == type_to_extension.end())
		return "";

	return type_to_extension.at(asset_type);
}

AssetType AssetManager::getAssetTypeFromExtension(const eastl::string &extension)
{
	static eastl::unordered_map<eastl::string, AssetType> extension_to_type =
	{
		{".dds", ASSET_TYPE_TEXTURE},
		{".png", ASSET_TYPE_TEXTURE},
		{".jpg", ASSET_TYPE_TEXTURE},
		{".fbx", ASSET_TYPE_MODEL},
		{".obj", ASSET_TYPE_MODEL},
	};

	if (extension_to_type.find(extension) == extension_to_type.end())
		return ASSET_TYPE_UNDEFINED;

	return extension_to_type.at(extension);
}

const AssetMetadata &AssetManager::getMetadata(const std::filesystem::path &source_path)
{
	return getMutableMetadata(source_path);
}

AssetMetadata &AssetManager::getMutableMetadata(const std::filesystem::path &source_path)
{
	for (auto &[handle, metadata] : registered_metadata)
	{
		if (metadata.source_path == source_path)
			return metadata;
	}
	return invalid_metadata;
}

void AssetManager::saveMetadata(const AssetMetadata &metadata)
{
	std::filesystem::path metadata_path = metadata.source_path;
	metadata_path.replace_extension(metadata_path.extension().string() + ".meta");

	YAML::Node node;
	node["guid"] = (uint64_t)metadata.asset_handle;
	node["runtime_guid"] = (uint64_t)metadata.runtime_handle;
	node["type"] = (int)metadata.type;
	node["Parameters"] = metadata.params;

	std::ofstream file(metadata_path);
	YAML::Emitter out(file);
	out << node;
}

bool AssetManager::isRuntimeExists(const std::filesystem::path &source_path)
{
	if (std::filesystem::exists(getRuntimeAssetPath(source_path)))
		return true;
	else
		return false;
}

void AssetManager::recreateRuntime(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;

	AssetType asset_type = metadata.type;
	if (asset_type == ASSET_TYPE_UNDEFINED)
		return;

	CORE_INFO("Recreating Runtime for {}", source_path.string());

	if (isRuntimeExists(source_path))
	{
		auto runtime_path = getRuntimeAssetPath(source_path);
		std::filesystem::remove(runtime_path);
	}

	if (asset_type == ASSET_TYPE_TEXTURE)
	{
		// Create runtime for it
		int generate_mipmaps = metadata.params["generate_mipmaps"].as<int>(1);

		Ref<Image> image = new Image(source_path.string().c_str());

		if (generate_mipmaps)
			image->createMipmaps();

		auto runtime_path = getRuntimeAssetPath(source_path);
		image->save(runtime_path);
		loaded_assets.erase(source_path.string().c_str());
		loaded_assets.erase(runtime_path.string().c_str());
		//AssetImporter::loadAsset(new_metadata, Ref<Asset>(image));
	} else if (asset_type == ASSET_TYPE_MODEL)
	{
		// Create runtime for it
		Ref<Model> model = new Model();
		model->load(source_path.string().c_str());

		auto runtime_path = getRuntimeAssetPath(source_path);
		model->saveFile(runtime_path.string().c_str());
		loaded_assets.erase(source_path.string().c_str());
		loaded_assets.erase(runtime_path.string().c_str());
	}
}

void AssetManager::reloadAsset(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;
	recreateRuntime(source_path);
}

void AssetManager::removeAsset(const std::filesystem::path &source_path)
{
	const AssetMetadata &metadata = getMetadata(source_path);
	if (!metadata.isValid())
		return;
	registered_metadata.erase(metadata.runtime_handle);
	loaded_assets.erase(source_path.string().c_str());
}

const AssetMetadata &AssetManager::importAsset(const std::filesystem::path &path)
{
	// Check if asset already registered
	const AssetMetadata &metadata = getMetadata(path);
	if (metadata.isValid())
		return metadata;


	// Check if metadata exists, if so, then load from file
	std::filesystem::path metadata_path = path;
	metadata_path.replace_extension(metadata_path.extension().string() + ".meta");
	if (std::filesystem::exists(metadata_path))
	{
		// TODO: load
		// If changed, recreate runtime
		YAML::Node node = YAML::LoadFile(metadata_path.string());

		YAML::Node params = node["Parameters"];

		AssetMetadata new_metadata;
		new_metadata.asset_handle = node["guid"].as<uint64_t>();
		new_metadata.runtime_handle = node["runtime_guid"].as<uint64_t>();
		new_metadata.source_path = path;
		new_metadata.type = (AssetType)node["type"].as<int>();

		registered_metadata[new_metadata.asset_handle] = new_metadata;

		if (!isRuntimeExists(path))
			recreateRuntime(path);
		return new_metadata;
	}

	AssetType asset_type = getAssetTypeFromExtension(path.extension().string().c_str());
	if (asset_type == ASSET_TYPE_UNDEFINED)
		return invalid_metadata;

	CORE_INFO("Reloading {}", path.string());

	// Else if metadata file not exists, create new metadata
	AssetMetadata new_metadata;
	new_metadata.asset_handle = Engine::GUID();
	new_metadata.runtime_handle = Engine::GUID();
	new_metadata.source_path = path;
	new_metadata.type = asset_type;

	if (asset_type == ASSET_TYPE_TEXTURE)
	{
		YAML::Node params;
		params["generate_mipmaps"] = 1;
		params["format"] = "FORMAT_R8G8B8A8_SRGB";
		new_metadata.params = params;

		saveMetadata(new_metadata);

		registered_metadata[new_metadata.asset_handle] = new_metadata;

		recreateRuntime(path);
	} else if (asset_type == ASSET_TYPE_MODEL)
	{
		YAML::Node params;
		new_metadata.params = params;

		saveMetadata(new_metadata);

		registered_metadata[new_metadata.asset_handle] = new_metadata;

		recreateRuntime(path);
	}
	return new_metadata;
}

void AssetManager::reloadAssets(const std::filesystem::path &path)
{
	// Go through all files and import them as assets
	for (auto entry : std::filesystem::directory_iterator(path))
	{
		if (entry.is_directory())
		{
			if (entry.path().filename() == ".runtimes")
				continue;
			reloadAssets(entry.path());
		} else
		{
			importAsset(entry.path());
		}
	}
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
