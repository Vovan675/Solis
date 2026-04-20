#pragma once
#include "FrameGraphBlackboard.h"
#include "RHI/DynamicRHI.h"
#include "FrameGraphRHIResources.h"
#include "FrameGraphPass.h"
#include "RenderPassResources.h"
#include "RenderPassBuilder.h"

class FrameGraph
{
public:
	FrameGraph()
	{
		all_textures.reserve(128);
		all_buffers.reserve(64);
		renderpass_nodes.reserve(128);
	}

	template <typename Data, typename Setup, typename Execute>
	Data addCallbackPass(eastl::string name, Setup setup, Execute execute)
	{
		PROFILE_CPU_SCOPE_VAR(("Setup: " + name).c_str());

		RenderPass<Data, Execute> *pass = new RenderPass<Data, Execute>(execute);
		int32_t node_id = renderpass_nodes.size();
		RenderPassNode &pass_node = renderpass_nodes.emplace_back(RenderPassNode(name, node_id, std::unique_ptr<RenderPass<Data, Execute>>(pass)));

		RenderPassBuilder builder(*this, pass_node);
		setup(builder, pass->data);

		return pass->data;
	}

	template <typename Setup, typename Execute>
	void addCallbackPass(eastl::string name, Setup setup, Execute execute)
	{
		PROFILE_CPU_SCOPE_VAR(("Setup: " + name).c_str());

		RenderPassNoData<Execute> *pass = new RenderPassNoData<Execute>(execute);
		int32_t node_id = renderpass_nodes.size();
		RenderPassNode &pass_node = renderpass_nodes.emplace_back(RenderPassNode(name, node_id, std::unique_ptr<RenderPassNoData<Execute>>(pass)));

		RenderPassBuilder builder(*this, pass_node);
		setup(builder);
	}

	FrameGraphTextureId importTexture(GraphicsResourceName name, RHITexture *texture)
	{
		uint32_t resource_id = all_textures.size();
		all_textures.emplace_back(FrameGraphTexture(resource_id, texture, name.name));
		texture_name_to_id[name] = resource_id;
		return resource_id;
	}

	FrameGraphBufferId importBuffer(GraphicsResourceName name, RHIBuffer *buffer)
	{
		uint32_t resource_id = all_buffers.size();
		all_buffers.emplace_back(FrameGraphBuffer(resource_id, buffer, name.name));
		buffer_name_to_id[name] = resource_id;
		return resource_id;
	}

	const FrameGraphBlackboard &getBlackboard() const { return blackboard; }
	FrameGraphBlackboard &getBlackboard() { return blackboard; }

	void compile();
	void execute(RHICommandList *cmd_list);

private:
	friend class GraphViz;
	friend class RenderPassBuilder;
	friend class RenderPassResources;

	FrameGraphTextureId createTextureResource(GraphicsResourceName name, const TextureDescription &desc)
	{
		uint32_t resource_id = all_textures.size();
		all_textures.emplace_back(FrameGraphTexture(resource_id, desc, name.name));
		texture_name_to_id[name] = resource_id;
		return resource_id;
	}

	FrameGraphBufferId createBufferResource(GraphicsResourceName name, const BufferDescription &desc)
	{
		uint32_t resource_id = all_buffers.size();
		all_buffers.emplace_back(FrameGraphBuffer(resource_id, desc, name.name));
		buffer_name_to_id[name] = resource_id;
		return resource_id;
	}

	TextureDescription getTextureDescription(GraphicsResourceName name)
	{
		FrameGraphTextureId resource_id = texture_name_to_id[name];
		return all_textures[resource_id.id].desc;
	}

	FrameGraphTexture *getFrameGraphTexture(FrameGraphTextureId id)
	{
		return &all_textures[id.id];
	}

	FrameGraphBuffer *getFrameGraphBuffer(FrameGraphBufferId id)
	{
		return &all_buffers[id.id];
	}

	eastl::vector<FrameGraphTexture> all_textures;
	eastl::vector<FrameGraphBuffer> all_buffers;

	eastl::unordered_map<GraphicsResourceName, FrameGraphTextureId> texture_name_to_id;
	eastl::unordered_map<GraphicsResourceName, FrameGraphBufferId> buffer_name_to_id;

	eastl::vector<RenderPassNode> renderpass_nodes;

	FrameGraphBlackboard blackboard;
};


template <typename Data, typename Execute>
struct RenderPass : RenderPassAbstract
{
	RenderPass(Execute execute): execute(execute) {}

	void operator()(const RenderPassResources &resources, RHICommandList *cmd_list) override
	{
		execute(data, resources, cmd_list);
	}

	Execute execute;
	Data data{};
};

template <typename Execute>
struct RenderPassNoData : RenderPassAbstract
{
	RenderPassNoData(Execute execute): execute(execute) {}

	void operator()(const RenderPassResources &resources, RHICommandList *cmd_list) override
	{
		execute(resources, cmd_list);
	}

	Execute execute;
};