#pragma once
#include "RHI/RHIDefinitions.h"
#include "Asset.h"
#include <filesystem>
#include <assimp/Importer.hpp>

class RHITexture;
class Model;
struct TextureDescription;

class AssetManager
{
public:
	static void init();
	static void shutdown();

	static RHITextureRef getTextureAsset(std::string path);
	static RHITextureRef getTextureAsset(std::string path, TextureDescription desc);
	static Ref<Model> getModelAsset(std::string path);

private:
	AssetManager() = delete;

	static Ref<Asset> load_texture_asset(std::string path, TextureDescription desc);
	static Ref<Asset> load_model_asset(std::string path);

	static std::unordered_map<std::string, Ref<Asset>> loaded_assets;
};