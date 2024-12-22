#pragma once
#include "RHI/Texture.h"

class TransientResources
{
public:
	static void init();
	static void cleanup();

	static void update();

	static std::shared_ptr<Texture> getTemporaryTexture(const TextureDescription &desc);
	static void releaseTemporaryTexture(std::shared_ptr<Texture> texture);

	struct ResourceEntry
	{
		std::shared_ptr<Texture> texture;
		uint32_t last_access_frame;
	};

	static std::unordered_map<size_t, std::vector<ResourceEntry>> textures;
};