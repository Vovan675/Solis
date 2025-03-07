#pragma once
#include "RHI/RHITexture.h"

class TransientResources
{
public:
	static void init();
	static void cleanup();

	static void update();

	static RHITexture *getTemporaryTexture(const TextureDescription &desc);
	static void releaseTemporaryTexture(RHITexture *texture);

	struct ResourceEntry
	{
		RHITextureRef texture;
		uint32_t last_access_frame;
		bool is_used;
	};

	static std::unordered_map<size_t, std::vector<ResourceEntry>> textures;
};