#include "pch.h"
#include "TransientResources.h"
#include "Rendering/Renderer.h"

std::unordered_map<size_t, std::vector<TransientResources::ResourceEntry>> TransientResources::textures;

void TransientResources::init()
{}

void TransientResources::cleanup()
{
	textures.clear();
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
		textures[hash].emplace_back(ResourceEntry{texture, gDynamicRHI->getFrame(), true});
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
