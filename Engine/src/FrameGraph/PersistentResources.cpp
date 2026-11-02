#include "pch.h"
#include "PersistentResources.h"

void PersistentResources::deleteUndeclared()
{
	eastl::erase_if(textures, [](const auto &entry) { return entry.second.last_declared_frame + 1 != gDynamicRHI->getFrame(); });
	eastl::erase_if(buffers, [](const auto &entry) { return entry.second.last_declared_frame + 1 != gDynamicRHI->getFrame(); });
}

bool PersistentResources::declareTexture(GraphicsResourceName name, const TextureDescription &desc)
{
	PersistentTexture &resource = textures[name];
	if (resource.declared_desc.getHash() != desc.getHash())
	{
		resource.texture = nullptr;
		resource.declared_desc = desc;
	}
	resource.last_declared_frame = gDynamicRHI->getFrame();
	return !resource.texture;
}

bool PersistentResources::declareBuffer(GraphicsResourceName name, const BufferDescription &desc)
{
	PersistentBuffer &resource = buffers[name];
	if (resource.declared_desc.getHash() != desc.getHash())
	{
		resource.buffer = nullptr;
		resource.declared_desc = desc;
	}
	resource.last_declared_frame = gDynamicRHI->getFrame();
	return !resource.buffer;
}

RHITexture *PersistentResources::getTexture(GraphicsResourceName name, const TextureDescription &desc, RHICommandList *cmd_list)
{
	PersistentTexture &resource = textures[name];
	if (!resource.texture)
	{
		resource.texture = gDynamicRHI->createTexture(desc);
		resource.texture->setDebugName(name.name);
		cmd_list->clearTexture(resource.texture, glm::vec4(0.0f));
	}
	return resource.texture;
}

RHIBuffer *PersistentResources::getBuffer(GraphicsResourceName name, const BufferDescription &desc, RHICommandList *cmd_list)
{
	PersistentBuffer &resource = buffers[name];
	if (!resource.buffer)
	{
		resource.buffer = gDynamicRHI->createBuffer(desc);
		resource.buffer->setDebugName(name.name);
		cmd_list->clearBuffer(resource.buffer, 0);
	}
	return resource.buffer;
}
