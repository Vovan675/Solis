#pragma once
#include "FrameGraphBlackboard.h"
#include "RHI/DynamicRHI.h"
#include <algorithm>
#include <string>

#include "FrameGraphRHIResources.h"
class FrameGraphTexture;

// Just a handle to any resource
struct FrameGraphTextureId 
{
	inline const static uint32_t invalid_id = uint32_t(-1);

	FrameGraphTextureId(): id(invalid_id) {}
	FrameGraphTextureId(uint32_t id): id(id) {}

	bool isValid() const { return id != invalid_id; }

	bool operator==(const FrameGraphTextureId &other) const
	{
		return other.id == id;
	}

	uint32_t id;
};

class RenderPassResources;
struct RenderPassAbstract
{
	virtual ~RenderPassAbstract() = default;
	virtual void operator()(const RenderPassResources &resources, RHICommandList *cmd_list) = 0;
};

class FrameGraphNode
{
public:
	FrameGraphNode() = delete;
	FrameGraphNode(const FrameGraphNode &) = delete;
	FrameGraphNode(FrameGraphNode &&) noexcept = default;
	virtual ~FrameGraphNode() = default;
	

	FrameGraphNode &operator=(const FrameGraphNode &) = delete;
	FrameGraphNode &operator=(FrameGraphNode &&) = delete;

	uint32_t getId() const { return id; }
	GraphicsResourceName getName() const { return name; }
	uint32_t getRefCount() const { return ref_count; }
protected:
	FrameGraphNode(const GraphicsResourceName name, uint32_t id): name(name), id(id) {}

	const uint32_t id;
	GraphicsResourceName name;
	uint32_t ref_count = 0;
};

class RenderPassNode final
{
public:
	RenderPassNode() = delete;
	RenderPassNode(const RenderPassNode &) = delete;
	RenderPassNode(RenderPassNode &&) noexcept = default;
	virtual ~RenderPassNode() = default;

	enum ResourceAccess: uint32_t
	{
		RESOURCE_ACCESS_NO_FLAG = 0,
		RESOURCE_ACCESS_IGNORE_FLAG = 1 << 0,
	};

	struct ResourceAccessDescription
	{
		FrameGraphTextureId resource = 0;
		uint32_t flags = 0;
	};

	RenderPassNode &operator=(const RenderPassNode &) = delete;
	RenderPassNode &operator=(RenderPassNode &&) = delete;

	bool isReading(FrameGraphTextureId resource) const
	{
		const auto match = [resource](const auto &e) { return e.resource == resource; };
		return eastl::find_if(reads.cbegin(), reads.cend(), match) != reads.cend();
	}

	bool isWriting(FrameGraphTextureId resource) const
	{
		const auto match = [resource](const auto &e) { return e.resource == resource; };
		return eastl::find_if(writes.cbegin(), writes.cend(), match) != writes.cend();
	}

	bool isCreating(FrameGraphTextureId resource) const
	{
		return eastl::find(creates.cbegin(), creates.cend(), resource) != creates.cend();
	}

	const auto &getCreates() const { return creates; }
	const auto &getReads() const { return reads; }
	const auto &getWrites() const { return writes; }

	bool hasSideEffect() const { return has_side_effect; }


	uint32_t getId() const { return id; }
	eastl::string getName() const { return name; }
	uint32_t getRefCount() const { return ref_count; }

private:
	friend class FrameGraph;
	friend class RenderPassBuilder;

	const uint32_t id;
	eastl::string name;
	uint32_t ref_count = 0;

	RenderPassNode(eastl::string name, uint32_t id, std::unique_ptr<RenderPassAbstract> &&pass)
		: name(name), id(id), pass(eastl::move(pass))
	{
		creates.reserve(8);
		reads.reserve(16);
		writes.reserve(8);
	}

	std::unique_ptr<RenderPassAbstract> pass; // TODO: this is error (leak memory)

	// Resources that were created by this pass
	eastl::vector<FrameGraphTextureId> creates;
	// Resources that needed read access to execute this pass
	eastl::vector<ResourceAccessDescription> reads;
	// Resources that needed write access to execute this pass
	eastl::vector<ResourceAccessDescription> writes;

	bool has_side_effect = false;
};

class ResourceNode final : public FrameGraphNode 
{
public:
	ResourceNode(const ResourceNode &) = delete;
	ResourceNode(ResourceNode &&) noexcept = default;

	ResourceNode &operator=(const ResourceNode &) = delete;
	ResourceNode &operator=(ResourceNode &&) = delete;

	FrameGraphTextureId getResource() const { return resource; }
	uint32_t getVersion() const { return version; }

private:
	friend class FrameGraph;

	ResourceNode(GraphicsResourceName name, uint32_t id, const FrameGraphTextureId &resource, uint32_t version)
		: FrameGraphNode(name, id), resource(resource), version(version)
	{}

	FrameGraphTextureId resource;
	uint32_t version;

	RenderPassNode *producer = nullptr;
};

class FrameGraph
{
public:
	FrameGraph()
	{
		all_textures.reserve(128);
		renderpass_nodes.reserve(128);
	}

private:

	FrameGraphTextureId createTextureResource(GraphicsResourceName name, const TextureDescription &desc, bool transient)
	{
		uint32_t resource_id = all_textures.size();
		all_textures.emplace_back(FrameGraphTexture(resource_id, desc, name.name));
		texture_name_to_id[name] = resource_id;

		return resource_id;
	}

	TextureDescription getTextureDescription(GraphicsResourceName name)
	{
		FrameGraphTextureId resource_id = texture_name_to_id[name];
		return all_textures[resource_id.id].desc;
	}

	TextureDescription getTextureDescription(FrameGraphTextureId id)
	{
		return all_textures[id.id].desc;
	}

	FrameGraphTexture *getFrameGraphTexture(FrameGraphTextureId id)
	{
		return &all_textures[id.id];
	}

public:
	template <typename Data, typename Setup, typename Execute>
	Data addCallbackPass(eastl::string name, Setup setup, Execute execute)
	{
		PROFILE_CPU_SCOPE_VAR(("Setup: " + name).c_str());
		RenderPass<Data, Execute> *pass = new RenderPass<Data, Execute>(execute);
		int32_t node_id = renderpass_nodes.size();
		RenderPassNode &pass_node = renderpass_nodes.emplace_back(RenderPassNode(name, node_id, std::unique_ptr<RenderPass<Data, Execute>>(pass))); // TODO: allocator for PassNodes

		RenderPassBuilder builder(*this, pass_node);

		
		setup(builder, pass->data);
		return pass->data;
	}

	// Used for importing persistent resources
	FrameGraphTextureId importTexture(GraphicsResourceName name, RHITexture *texture)
	{
		uint32_t resource_id = all_textures.size();
		all_textures.emplace_back(FrameGraphTexture(resource_id, texture, name.name));
		texture_name_to_id[name] = resource_id;
		return resource_id;
	}

	const FrameGraphBlackboard &getBlackboard() const { return blackboard; }
	FrameGraphBlackboard &getBlackboard() { return blackboard; }

	void compile();

	// Go through pases and execute them
	void execute(RHICommandList *cmd_list);

private:
	friend class GraphViz;
	friend class RenderPassBuilder;
	friend class RenderPassResources;


	eastl::vector<FrameGraphTexture> all_textures;

	eastl::unordered_map<GraphicsResourceName, FrameGraphTextureId> texture_name_to_id;

	eastl::vector<RenderPassNode> renderpass_nodes;

	FrameGraphBlackboard blackboard;
};


class RenderPassResources
{
public:
	RHITexture* getTexture(FrameGraphTextureId resource) const
	{
		return frameGraph.all_textures[resource.id].texture;
	}

	RHITexture* getTexture(GraphicsResourceName name) const
	{
		FrameGraphTextureId resource_id = frameGraph.texture_name_to_id[name];
		return frameGraph.all_textures[resource_id.id].texture;
	}

	uint32_t getBindlessId(FrameGraphTextureId resource) const
	{
		return frameGraph.all_textures[resource.id].getBindlessId();
	}

	uint32_t getBindlessId(GraphicsResourceName name) const
	{
		FrameGraphTextureId resource_id = frameGraph.texture_name_to_id[name];
		return frameGraph.all_textures[resource_id.id].getBindlessId();
	}

	bool has(GraphicsResourceName name) const
	{
		return frameGraph.texture_name_to_id.contains(name);
	}
private:
	friend class FrameGraph;

	RenderPassResources(FrameGraph &frameGraph, const RenderPassNode &pass) : frameGraph(frameGraph), pass(pass) {}
	FrameGraph &frameGraph;
	const RenderPassNode &pass;
};

// Just one pass that has data and renders using it
template <typename Data, typename Execute>
struct RenderPass : RenderPassAbstract
{
	RenderPass(Execute execute): execute(execute)
	{}

	void operator()(const RenderPassResources &resources, RHICommandList *cmd_list) override
	{
		execute(data, resources, cmd_list);
	}

	Execute execute;
	Data data{};
};


// Used during setup phase, created by frame graph, creates one PassNode
class RenderPassBuilder
{
public:
	FrameGraphTextureId read(FrameGraphTextureId resource, uint32_t flags = 0)
	{
		return renderpass_node.reads.emplace_back(RenderPassNode::ResourceAccessDescription{resource, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
	}
	FrameGraphTextureId readTexture(FrameGraphTextureId texture, uint32_t flags = 0)
	{
		FrameGraphTextureId resource = renderpass_node.reads.emplace_back(RenderPassNode::ResourceAccessDescription{texture, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
		return resource;
	}

	FrameGraphTextureId readTexture(GraphicsResourceName texture, uint32_t flags = 0)
	{
		return readTexture(frameGraph.texture_name_to_id[texture], flags);
	}

	FrameGraphTextureId readDepthTexture(GraphicsResourceName texture, uint32_t flags = 0)
	{
		return readTexture(frameGraph.texture_name_to_id[texture], flags | TEXTURE_RESOURCE_ACCESS_READ_ONLY_DEPTH);
	}

	FrameGraphTextureId writeTexture(FrameGraphTextureId texture, uint32_t flags = 0)
	{
		frameGraph.all_textures[texture.id].desc.usage_flags |= TEXTURE_USAGE_ATTACHMENT;

		FrameGraphTextureId resource;
		if (renderpass_node.isCreating(texture))
		{
			resource = renderpass_node.writes.emplace_back(RenderPassNode::ResourceAccessDescription{texture, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
		} else
		{
			// When writing to already created resource, we need to clone it and rename for better error handling
			renderpass_node.reads.emplace_back(RenderPassNode::ResourceAccessDescription{texture, RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG | flags}).resource;
			resource = renderpass_node.writes.emplace_back(RenderPassNode::ResourceAccessDescription{texture, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
		}
		return resource;
	}

	FrameGraphTextureId writeTexture(GraphicsResourceName name, uint32_t flags = 0)
	{
		return writeTexture(frameGraph.texture_name_to_id[name], flags);
	}

	FrameGraphTextureId writeUAVTexture(FrameGraphTextureId texture, uint32_t flags = 0)
	{
		frameGraph.all_textures[texture.id].desc.usage_flags |= TEXTURE_USAGE_STORAGE;

		FrameGraphTextureId resource;
		if (renderpass_node.isCreating(texture))
		{
			resource = renderpass_node.writes.emplace_back(RenderPassNode::ResourceAccessDescription{texture, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
		} else
		{
			// When writing to already created resource, we need to clone it and rename for better error handling
			renderpass_node.reads.emplace_back(RenderPassNode::ResourceAccessDescription{texture, RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG | flags}).resource;
			resource = renderpass_node.writes.emplace_back(RenderPassNode::ResourceAccessDescription{texture, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
		}
		return resource;
	}

	FrameGraphTextureId writeUAVTexture(GraphicsResourceName name, uint32_t flags = 0)
	{
		return writeUAVTexture(frameGraph.texture_name_to_id[name], flags);
	}

	bool isTextureCreated(GraphicsResourceName name)
	{
		return frameGraph.texture_name_to_id.contains(name);
	}

	FrameGraphTextureId createTexture(eastl::string name, uint32_t width, uint32_t height, Format format)
	{
		TextureDescription desc;
		desc.width = width;
		desc.height = height;
		desc.format = format;

		FrameGraphTextureId resource_id = frameGraph.createTextureResource(GraphicsResourceName(name.c_str()), desc, true);
		renderpass_node.creates.emplace_back(resource_id);
		return resource_id;
	}

	FrameGraphTextureId createTexture(GraphicsResourceName name, uint32_t width, uint32_t height, Format format)
	{
		TextureDescription desc;
		desc.width = width;
		desc.height = height;
		desc.format = format;

		FrameGraphTextureId resource_id = frameGraph.createTextureResource(name, desc, true);
		renderpass_node.creates.emplace_back(resource_id);
		return resource_id;
	}

	FrameGraphTextureId createTexture(GraphicsResourceName name, TextureDescription desc)
	{
		FrameGraphTextureId resource_id = frameGraph.createTextureResource(name, desc, true);
		renderpass_node.creates.emplace_back(resource_id);
		return resource_id;
	}

	TextureDescription getTextureDescription(GraphicsResourceName name)
	{
		return frameGraph.getTextureDescription(name);
	}

	TextureDescription getTextureDescription(FrameGraphTextureId id)
	{
		return frameGraph.getTextureDescription(id);
	}

	void setSideEffect(bool side_effect)
	{
		renderpass_node.has_side_effect = side_effect;
	}
private:
	friend class FrameGraph;

	RenderPassBuilder(FrameGraph &frameGraph, RenderPassNode &renderpass_node) : frameGraph(frameGraph), renderpass_node(renderpass_node) {}
	FrameGraph &frameGraph;
	RenderPassNode &renderpass_node;
};