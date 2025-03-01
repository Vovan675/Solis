#pragma once
#include "RHI/RHITexture.h"

class TransientResources
{
public:
	static void init();
	static void cleanup();

	static void update();

	static std::shared_ptr<RHITexture> getTemporaryTexture(const TextureDescription &desc);
	static void releaseTemporaryTexture(std::shared_ptr<RHITexture> texture);

	struct ResourceEntry
	{
		std::shared_ptr<RHITexture> texture;
		uint32_t last_access_frame;
	};

	static std::unordered_map<size_t, std::vector<ResourceEntry>> textures;
};