#include "pch.h"
#include "FrameGraph.h"
#include "PersistentResources.h"
#include "TransientResources.h"
#include "Rendering/Renderer.h"

bool FrameGraph::createPersistentTexture(GraphicsResourceName name, const TextureDescription &desc)
{
	if (!texture_name_to_id.contains(name))
	{
		FrameGraphTextureId resource_id = createTextureResource(name, desc);
		all_textures[resource_id.id].is_transient = false;
		persistent_texture_names.push_back(name);
	}
	return persistent_resources.declareTexture(name, desc);
}

bool FrameGraph::createPersistentBuffer(GraphicsResourceName name, const BufferDescription &desc)
{
	if (!buffer_name_to_id.contains(name))
	{
		FrameGraphBufferId resource_id = createBufferResource(name, desc);
		all_buffers[resource_id.id].is_transient = false;
		persistent_buffer_names.push_back(name);
	}
	return persistent_resources.declareBuffer(name, desc);
}

void FrameGraph::compile()
{
	for (auto &pass : renderpass_nodes)
	{
		// All resources that written by it, are references it
		pass.ref_count = pass.texture_writes.size() + pass.buffer_writes.size() ;
		
		// All resources that are read, has +1 references to them
		for (auto &read_access : pass.texture_reads)
		{
			FrameGraphTexture *read_resource = getFrameGraphTexture(read_access);
			read_resource->ref_count++;
		}

		for (auto &write_access : pass.texture_writes)
		{
			FrameGraphTexture *write_resource = getFrameGraphTexture(write_access);
			write_resource->producer = &pass;
		}
		
		for (auto &read_access : pass.buffer_reads)
		{
			FrameGraphBuffer *read_resource = getFrameGraphBuffer(read_access);
			read_resource->ref_count++;
		}

		for (auto &write_access : pass.buffer_writes)
		{
			FrameGraphBuffer *write_resource = getFrameGraphBuffer(write_access);
			write_resource->producer = &pass;
		}
	}

	// Cull
	eastl::vector<FrameGraphTexture *> unreferenced_resources;
	#ifndef DEBUG
	for (auto &resource : all_textures)
	{
		if (resource.ref_count == 0 && resource.is_transient)
			unreferenced_resources.push_back(&resource);
	}
	#endif

	while (!unreferenced_resources.empty())
	{
		auto resource = unreferenced_resources.back();
		unreferenced_resources.pop_back();
		RenderPassNode *producer = resource->producer;
		if (!producer || producer->hasSideEffect())
			continue;
		
		producer->ref_count--;
		if (producer->getRefCount() == 0)
		{
			for (auto &read_access : producer->texture_reads)
			{
				FrameGraphTexture *read_resource = getFrameGraphTexture(read_access);
				read_resource->ref_count--;

				if (read_resource->ref_count == 0)
					unreferenced_resources.push_back(read_resource);
			}
		}
	}

	// Needed info for lifetime management
	for (auto &pass : renderpass_nodes)
	{
		if (pass.ref_count == 0 && !pass.has_side_effect)
			continue;

		for (auto &id : pass.texture_creates)
			getFrameGraphTexture(id)->producer = &pass;

		for (auto &access : pass.texture_reads)
			getFrameGraphTexture(access)->last_consumer = &pass;

		for (auto &access : pass.texture_writes)
			getFrameGraphTexture(access)->last_consumer = &pass;

		for (auto &id : pass.buffer_creates)
			getFrameGraphBuffer(id)->producer = &pass;

		for (auto &access : pass.buffer_reads)
			getFrameGraphBuffer(access)->last_consumer = &pass;

		for (auto &access : pass.buffer_writes)
			getFrameGraphBuffer(access)->last_consumer = &pass;
	}
}

void FrameGraph::execute(RHICommandList *cmd_list)
{
	// Persistent resources
	for (GraphicsResourceName name : persistent_texture_names)
	{
		FrameGraphTexture &texture = all_textures[texture_name_to_id[name].id];
		texture.resource = persistent_resources.getTexture(name, texture.desc, cmd_list);
	}

	for (GraphicsResourceName name : persistent_buffer_names)
	{
		FrameGraphBuffer &buffer = all_buffers[buffer_name_to_id[name].id];
		buffer.resource = persistent_resources.getBuffer(name, buffer.desc, cmd_list);
	}

	// Per pass work
	for (const auto &pass : renderpass_nodes)
	{
		if (pass.getRefCount() == 0 && !pass.has_side_effect)
			continue;

		PROFILE_CPU_SCOPE_VAR(pass.getName().c_str());
		PROFILE_GPU_SCOPE_VAR(cmd_list, pass.getName().c_str());

		for (const auto &id : pass.texture_creates)
		{
			FrameGraphTexture *texture = getFrameGraphTexture(id);
			texture->resource = transient_resources.getTexture(texture->desc);
			texture->resource->setDebugName(texture->name.c_str());
		}
		for (const auto &[id, usage] : pass.texture_usage)
			getFrameGraphTexture(id)->resource->transitState(cmd_list, usage);

		for (const auto &id : pass.buffer_creates)
		{
			FrameGraphBuffer *buffer = getFrameGraphBuffer(id);
			buffer->resource = transient_resources.getBuffer(buffer->desc);
			buffer->resource->setDebugName(buffer->name.c_str());
		}

		for (const auto &[id, usage] : pass.buffer_usage)
			getFrameGraphBuffer(id)->resource->transitState(cmd_list, usage);

		{	
			RenderPassResources resources(*this, pass);
			std::invoke(*pass.pass, resources, cmd_list);
		}

		for (auto &entry: all_textures)
		{
			if (entry.last_consumer == &pass && entry.is_transient)
				transient_resources.releaseTexture(eastl::move(entry.resource));
		}

		for (auto &entry: all_buffers)
		{
			if ((entry.last_consumer == &pass) && entry.is_transient)
				transient_resources.releaseBuffer(eastl::move(entry.resource));
		}
	}
}

