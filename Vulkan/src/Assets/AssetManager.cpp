#include "pch.h"
#include "AssetManager.h"
#include "RHI/RHITexture.h"
#include "Model.h"

std::unordered_map<std::string, Ref<Asset>> AssetManager::loaded_assets;

void AssetManager::init()
{

}

void AssetManager::shutdown()
{
	loaded_assets.clear();
}

RHITextureRef AssetManager::getTextureAsset(std::string path)
{
	TextureDescription tex_description{};
	tex_description.format = FORMAT_R8G8B8A8_SRGB;
	tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;
	return getTextureAsset(path, tex_description);
}

RHITextureRef AssetManager::getTextureAsset(std::string path, TextureDescription desc)
{
	if (loaded_assets.find(path) != loaded_assets.end())
		return loaded_assets[path];

	Ref<Asset> new_asset;

	auto std_path = std::filesystem::path(path);
	std::string extension = std_path.extension().string();
	new_asset = load_texture_asset(path, desc);

	loaded_assets[path] = new_asset;
	return new_asset;
}

Ref<Model> AssetManager::getModelAsset(std::string path)
{
	if (loaded_assets.find(path) != loaded_assets.end())
		return loaded_assets[path];

	Ref<Asset> new_asset;

	auto std_path = std::filesystem::path(path);
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

Ref<Asset> AssetManager::load_texture_asset(std::string path, TextureDescription desc)
{
	auto tex = gDynamicRHI->createTexture(desc);
	tex->load(path.c_str());
	return tex;
}

Ref<Asset> AssetManager::load_model_asset(std::string path)
{
	auto model = new Model();
	model->load(path.c_str());
	return model;
}
