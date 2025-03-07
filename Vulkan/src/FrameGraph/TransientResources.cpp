#include "pch.h"
#include "TransientResources.h"
#include "Rendering/Renderer.h"

std::unordered_map<size_t, std::vector<TransientResources::ResourceEntry>> TransientResources::textures;

void TransientResources::init()
{}

void TransientResources::cleanup()
{
	for (auto &vec : textures)
	{
		for (auto &resource : vec.second)
		{
			delete resource.texture;
		}
	}
	textures.clear();
}

void TransientResources::update()
{
	for (auto it = textures.begin(); it != textures.end(); it++)
	{
		for (auto resource_it = it->second.begin(); resource_it != it->second.end(); resource_it++)
		{
			if (Renderer::getCurrentFrame() - resource_it->last_access_frame > 10)
			{
				delete resource_it->texture;
				resource_it = it->second.erase(resource_it);
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
				entry.last_access_frame = Renderer::getCurrentFrame();
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
		textures[hash].emplace_back(ResourceEntry{texture, Renderer::getCurrentFrame(), true});
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
			entry.last_access_frame = Renderer::getCurrentFrame();
			entry.is_used = false;
		}
	}
}
