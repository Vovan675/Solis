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
			if (Renderer::getCurrentFrame() - resource_it->last_access_frame > 1)
			{
				resource_it = it->second.erase(resource_it);
				if (it->second.empty())
					break;
			}
		}
	}
}

std::shared_ptr<Texture> TransientResources::getTemporaryTexture(const TextureDescription &desc)
{
	size_t hash = desc.getHash();
	if (textures.find(hash) != textures.end() && !textures[hash].empty())
	{
		auto resource = textures[hash].back();
		textures[hash].pop_back();
		return resource.texture;
	} else
	{
		auto texture = Texture::create(desc);
		return texture;
	}
}

void TransientResources::releaseTemporaryTexture(std::shared_ptr<Texture> texture)
{
	size_t hash = texture->getDescription().getHash();
	textures[hash].emplace_back(ResourceEntry{texture, Renderer::getCurrentFrame()});
}
