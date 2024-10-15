#pragma once
#include "Asset.h"
#include <filesystem>
#include <assimp/Importer.hpp>

class Texture;
class Model;
struct TextureDescription;

class AssetManager
{
public:
	static void init();
	static void shutdown();

	static std::shared_ptr<Texture> getTextureAsset(std::string path);
	static std::shared_ptr<Texture> getTextureAsset(std::string path, TextureDescription desc);
	static std::shared_ptr<Model> getModelAsset(std::string path);

private:
	AssetManager() = delete;

	static std::shared_ptr<Asset> load_texture_asset(std::string path, TextureDescription desc);
	static std::shared_ptr<Asset> load_model_asset(std::string path);

	static std::unordered_map<std::string, std::shared_ptr<Asset>> loaded_assets;
};