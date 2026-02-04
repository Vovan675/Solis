#include "pch.h"
#include "TransientResources.h"
#include "Rendering/Renderer.h"

eastl::unordered_map<size_t, eastl::vector<TransientResources::ResourceEntry>> TransientResources::textures;
eastl::unordered_map<size_t, eastl::vector<TransientResources::ResourceEntry>> TransientResources::buffers;

void TransientResources::init()
{}

void TransientResources::cleanup()
{
	textures.clear();
	buffers.clear();
}

void TransientResources::update()
{
	for (auto it = textures.begin(); it != textures.end(); it++)
	{
		for (auto resource_it = it->second.begin(); resource_it != it->second.end(); resource_it++)
		{
			if (gDynamicRHI->getFrame() - resource_it->last_access_frame > 10)
			{
				resource_it = it->second.erase(resource_it);
				if (resource_it >= it->second.end())
					break;
				if (it->second.empty())
					break;
			}
		}
	}
}

RHITexture *TransientResources::getTemporaryTexture(const TextureDescription &desc)
{
	size_t hash = desc.getHash();

	RHITexture *temporary_texture = nullptr;

	// Try to find unused temporary texture
	if (textures.find(hash) != textures.end())
	{
		for (auto &entry : textures[hash])
		{
			if (!entry.is_used)
			{
				entry.last_access_frame = gDynamicRHI->getFrame();
				entry.is_used = true;
				temporary_texture = entry.texture;
				break;
			}
		}
	}

	// Create new texture if not exists
	if (!temporary_texture)
	{
		auto texture = gDynamicRHI->createTexture(desc);
		textures[hash].emplace_back(texture, gDynamicRHI->getFrame(), true);
		return texture;
	}

	return temporary_texture;
}

void TransientResources::releaseTemporaryTexture(RHITexture *texture)
{
	size_t hash = texture->getDescription().getHash();
	for (auto &entry : textures[hash])
	{
		if (entry.texture == texture)
		{
			entry.last_access_frame = gDynamicRHI->getFrame();
			entry.is_used = false;
		}
	}
}

RHIBuffer *TransientResources::getTemporaryBuffer(const BufferDescription &desc)
{
	size_t hash = desc.getHash();

	RHIBuffer *temporary_buffer = nullptr;

	// Try to find unused temporary buffer
	if (buffers.find(hash) != buffers.end())
	{
		for (auto &entry : buffers[hash])
		{
			if (!entry.is_used)
			{
				entry.last_access_frame = gDynamicRHI->getFrame();
				entry.is_used = true;
				temporary_buffer = entry.buffer;
				break;
			}
		}
	}

	// Create new buffer if not exists
	if (!temporary_buffer)
	{
		auto buffer = gDynamicRHI->createBuffer(desc);
		buffers[hash].emplace_back(buffer, gDynamicRHI->getFrame(), true);
		return buffer;
	}

	return temporary_buffer;
}

void TransientResources::releaseTemporaryBuffer(RHIBuffer *buffer)
{
	size_t hash = buffer->getDescription().getHash();
	for (auto &entry : buffers[hash])
	{
		if (entry.buffer == buffer)
		{
			entry.last_access_frame = gDynamicRHI->getFrame();
			entry.is_used = false;
		}
	}
}
