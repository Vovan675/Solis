#include "pch.h"
#include "TransientResources.h"

void TransientResources::deleteUnused()
{
	for (auto &[hash, free_textures] : textures)
		eastl::erase_if(free_textures, [](const TransientTexture &transient) { return transient.last_frame + 1 != gDynamicRHI->getFrame(); });
	for (auto &[hash, free_buffers] : buffers)
		eastl::erase_if(free_buffers, [](const TransientBuffer &transient) { return transient.last_frame + 1 != gDynamicRHI->getFrame(); });
}

RHITextureRef TransientResources::getTexture(const TextureDescription &desc)
{
	eastl::vector<TransientTexture> &free_textures = textures[desc.getHash()];

	// Create new if no free textures
	if (free_textures.empty())
	{
		return gDynamicRHI->createTexture(desc);
	}

	// Return last free texture
	RHITextureRef texture = free_textures.back().texture;
	free_textures.pop_back();
	return texture;
}

void TransientResources::releaseTexture(RHITextureRef texture)
{
	TransientTexture &transient = textures[texture->getDescription().getHash()].push_back();
	transient.texture = texture;
	transient.last_frame = gDynamicRHI->getFrame();
}

RHIBufferRef TransientResources::getBuffer(const BufferDescription &desc)
{
	eastl::vector<TransientBuffer> &free_buffers = buffers[desc.getHash()];

	// Create new if no free buffers
	if (free_buffers.empty())
		return gDynamicRHI->createBuffer(desc);

	// Return last free buffer
	RHIBufferRef buffer = free_buffers.back().buffer;
	free_buffers.pop_back();
	return buffer;
}

void TransientResources::releaseBuffer(RHIBufferRef buffer)
{
	TransientBuffer &transient = buffers[buffer->getDescription().getHash()].push_back();
	transient.buffer = buffer;
	transient.last_frame = gDynamicRHI->getFrame();
}
