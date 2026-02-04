#pragma once
#include "RHI/RHITexture.h"
#include "RHI/RHIBuffer.h"

class TransientResources
{
public:
	static void init();
	static void cleanup();

	static void update();

	static RHITexture *getTemporaryTexture(const TextureDescription &desc);
	static void releaseTemporaryTexture(RHITexture *texture);

	static RHIBuffer *getTemporaryBuffer(const BufferDescription &desc);
	static void releaseTemporaryBuffer(RHIBuffer *buffer);

	struct ResourceEntry
	{
		ResourceEntry(RHITextureRef texture, uint64_t last_access_frame, bool is_used): texture(texture), last_access_frame(last_access_frame), is_used(is_used) {}
		ResourceEntry(RHIBufferRef buffer, uint64_t last_access_frame, bool is_used): buffer(buffer), last_access_frame(last_access_frame), is_used(is_used) {}

		RHITextureRef texture;
		RHIBufferRef buffer;
		uint64_t last_access_frame;
		bool is_used;
	};

	static eastl::unordered_map<size_t, eastl::vector<ResourceEntry>> textures;
	static eastl::unordered_map<size_t, eastl::vector<ResourceEntry>> buffers;
};