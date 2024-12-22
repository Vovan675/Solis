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
			auto &read_resource_node = resource_nodes[read_access.resource];
			read_resource_node.ref_count++;
		}

		for (auto &write_access : pass.writes)
		{
			auto &write_resource_node = resource_nodes[write_access.resource];
			write_resource_node.producer = &pass;
		}
	}

	// Cull
	std::vector<ResourceNode *> unreferenced_resources;
	for (auto &resource_node : resource_nodes)
	{
		if (resource_node.ref_count == 0)
			unreferenced_resources.push_back(&resource_node);
	}

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
				auto &read_resource_node = resource_nodes[read_access.resource];
				read_resource_node.ref_count--;

				if (read_resource_node.ref_count == 0)
					unreferenced_resources.push_back(&read_resource_node);
			}
		}
	}


	// Needed info for lifetime management
	for (auto &pass : renderpass_nodes)
	{
		if (pass.ref_count == 0)
			continue;

		for (auto &node_id : pass.creates)
			getResourceEntry(node_id).producer = &pass;

		for (auto &node_id : pass.reads)
			getResourceEntry(node_id.resource).last_consumer = &pass;

		for (auto &node_id : pass.writes)
			getResourceEntry(node_id.resource).last_consumer = &pass;
	}
}

void FrameGraph::execute(CommandBuffer &command_buffer)
{
	for (const auto &pass : renderpass_nodes)
	{
		if (pass.getRefCount() == 0)
			continue;

		for (const auto &node_id : pass.creates)
			getResourceEntry(node_id).create();

		for (const auto &access : pass.reads)
		{
			if ((access.flags & RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG) == 0)
				getResourceEntry(access.resource).preRead(command_buffer, access.flags);
		}
		for (const auto &access : pass.writes)
		{
			if ((access.flags & RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG) == 0)
				getResourceEntry(access.resource).preWrite(command_buffer, access.flags);
		}

		{
			GPU_SCOPE(pass.getName().c_str(), &command_buffer);
			RenderPassResources resources(*this, pass);
			std::invoke(*pass.pass, resources, command_buffer);
		}

		for (auto &entry: resource_registry)
		{
			if (entry.last_consumer == &pass && entry.isTransient())
				entry.destroy();
		}
	}
}

