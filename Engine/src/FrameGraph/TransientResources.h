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
		uint64_t last_access_frame;
		bool is_used;
	};

	static eastl::unordered_map<size_t, eastl::vector<ResourceEntry>> textures;
};