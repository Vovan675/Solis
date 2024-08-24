#include "pch.h"
#include "AssetManager.h"
#include "RHI/Texture.h"
#include "Model.h"

std::unordered_map<std::string, std::shared_ptr<Asset>> AssetManager::loaded_assets;

void AssetManager::init()
{

}

void AssetManager::shutdown()
{
	loaded_assets.clear();
}

std::shared_ptr<Asset> AssetManager::load_texture_asset(std::string path)
{
	TextureDescription tex_description{};
	tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	auto tex = std::make_shared<Texture>(tex_description);
	tex->load(path.c_str());
	return std::dynamic_pointer_cast<Asset>(tex);
}

std::shared_ptr<Asset> AssetManager::load_model_asset(std::string path)
{
	auto model = std::make_shared<Model>();
	model->load(path.c_str());
	return std::dynamic_pointer_cast<Asset>(model);
}
