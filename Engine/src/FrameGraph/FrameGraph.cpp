#include "pch.h"
#include "FrameGraph.h"
#include "Rendering/Renderer.h"

void FrameGraph::compile()
{
	for (auto &pass : renderpass_nodes)
	{
		// All resources that written by it, are references it
		pass.ref_count = pass.writes.size();
		
		// All resources that are read, has +1 references to them
		for (auto &read_access : pass.reads)
		{
			FrameGraphTexture *read_resource = getFrameGraphTexture(read_access.resource);
			read_resource->ref_count++;
		}

		for (auto &write_access : pass.writes)
		{
			FrameGraphTexture *write_resource = getFrameGraphTexture(write_access.resource);
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
			for (auto &read_access : producer->reads)
			{
				FrameGraphTexture *read_resource = getFrameGraphTexture(read_access.resource);
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

		for (auto &id : pass.creates)
			getFrameGraphTexture(id)->producer = &pass;

		for (auto &access : pass.reads)
			getFrameGraphTexture(access.resource)->last_consumer = &pass;

		for (auto &access : pass.writes)
			getFrameGraphTexture(access.resource)->last_consumer = &pass;
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

		for (const auto &id : pass.creates)
			getFrameGraphTexture(id)->create();

		for (const auto &access : pass.reads)
		{
			if ((access.flags & RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG) == 0)
				getFrameGraphTexture(access.resource)->preRead(cmd_list, access.flags);
		}
		for (const auto &access : pass.writes)
		{
			if ((access.flags & RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG) == 0)
				getFrameGraphTexture(access.resource)->preWrite(cmd_list, access.flags);
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
	}
}

