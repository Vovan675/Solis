#include "pch.h"
#include "FrameGraph.h"
#include "Rendering/Renderer.h"

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
		if (resource.ref_count == 0)
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
	for (const auto &pass : renderpass_nodes)
	{
		if (pass.getRefCount() == 0 && !pass.has_side_effect)
			continue;

		PROFILE_CPU_SCOPE_VAR(pass.getName().c_str());
		PROFILE_GPU_SCOPE_VAR(cmd_list, pass.getName().c_str());

		for (const auto &id : pass.texture_creates)
			getFrameGraphTexture(id)->create();
		for (const auto &id : pass.texture_reads)
		{
			ResourceState usage = pass.texture_usage.at(id);
			getFrameGraphTexture(id)->preRead(cmd_list, usage);
		}
		for (const auto &id : pass.texture_writes)
		{
			ResourceState usage = pass.texture_usage.at(id);
			getFrameGraphTexture(id)->preWrite(cmd_list, usage);
		}

		for (const auto &id : pass.buffer_creates)
			getFrameGraphBuffer(id)->create();

		for (const auto &id : pass.buffer_reads)
		{
			ResourceState usage = pass.buffer_usage.at(id);
			getFrameGraphBuffer(id)->preRead(cmd_list, usage);
		}
		for (const auto &id : pass.buffer_writes)
		{
			ResourceState usage = pass.buffer_usage.at(id);
			getFrameGraphBuffer(id)->preWrite(cmd_list, usage);
		}

		{	
			RenderPassResources resources(*this, pass);
			std::invoke(*pass.pass, resources, cmd_list);
		}

		for (auto &entry: all_textures)
		{
			if (entry.last_consumer == &pass && entry.is_transient)
				entry.destroy();
		}

		for (auto &entry: all_buffers)
		{
			if ((entry.last_consumer == &pass) && entry.is_transient)
				entry.destroy();
		}
	}
}

