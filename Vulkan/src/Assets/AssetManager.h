#pragma once
#include "Asset.h"
#include <filesystem>
#include <assimp/Importer.hpp>

class AssetManager
{
public:
	static void init();
	static void shutdown();

	template <typename AssetType>
	static std::shared_ptr<AssetType> getAsset(std::string path)
	{
		if (loaded_assets.find(path) != loaded_assets.end())
			return std::dynamic_pointer_cast<AssetType>(loaded_assets[path]);

		std::shared_ptr<Asset> new_asset;

		auto std_path = std::filesystem::path(path);
		std::string extension = std_path.extension().string();
		if (extension == ".png" || extension == ".jpg" || extension == ".hdr" || extension == ".dds")
		{
			new_asset = load_texture_asset(path);
		} else if (Assimp::Importer().IsExtensionSupported(extension))
		{
			new_asset = load_model_asset(path);
		} else
		{
			CORE_ERROR("Asset Type not supported %s", path.c_str());
			return nullptr;
		}
		
		loaded_assets[path] = new_asset;
		return std::dynamic_pointer_cast<AssetType>(new_asset);
	}

private:
	AssetManager() = delete;

	static std::shared_ptr<Asset> load_texture_asset(std::string path);
	static std::shared_ptr<Asset> load_model_asset(std::string path);

	static std::unordered_map<std::string, std::shared_ptr<Asset>> loaded_assets;
};