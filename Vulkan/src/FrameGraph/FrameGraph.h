#pragma once
#include "FrameGraphBlackboard.h"
#include "RHI/CommandBuffer.h"
#include <algorithm>

// Just a handle to any resource
using FrameGraphResource = int32_t;

struct EngineTexture
{
	int colorRed = 0;
};




class RenderPassResources;
struct RenderPassAbstract
{
	virtual void operator()(const RenderPassResources &resources, CommandBuffer &command_buffer) = 0;
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
	std::string getName() const { return name; }
	uint32_t getRefCount() const { return ref_count; }
protected:
	FrameGraphNode(const std::string name, uint32_t id): name(name), id(id) {}

private:
	friend class FrameGraph;

	const uint32_t id;
	std::string name;
	uint32_t ref_count = 0;
};

class RenderPassNode final : public FrameGraphNode 
{
public:
	enum ResourceAccess: uint32_t
	{
		RESOURCE_ACCESS_NO_FLAG = 0,
		RESOURCE_ACCESS_IGNORE_FLAG = 1 << 0,
	};

	struct ResourceAccessDescription
	{
		FrameGraphResource resource = 0;
		uint32_t flags = 0;
	};

	RenderPassNode(const RenderPassNode &) = delete;
	RenderPassNode(RenderPassNode &&) noexcept = default;

	RenderPassNode &operator=(const RenderPassNode &) = delete;
	RenderPassNode &operator=(RenderPassNode &&) = delete;

	bool isReading(FrameGraphResource resource) const
	{
		const auto match = [resource](const auto &e) { return e.resource == resource; };
		return std::find_if(reads.cbegin(), reads.cend(), match) != reads.cend();
	}

	bool isWriting(FrameGraphResource resource) const
	{
		const auto match = [resource](const auto &e) { return e.resource == resource; };
		return std::find_if(writes.cbegin(), writes.cend(), match) != writes.cend();
	}

	bool isCreating(FrameGraphResource resource) const
	{
		return std::find(creates.cbegin(), creates.cend(), resource) != creates.cend();
	}

	const auto &getCreates() const { return creates; }
	const auto &getReads() const { return reads; }
	const auto &getWrites() const { return writes; }

	bool hasSideEffect() const { return has_side_effect; }

private:
	friend class FrameGraph;
	friend class RenderPassBuilder;

	RenderPassNode(std::string name, uint32_t id, std::unique_ptr<RenderPassAbstract> &&pass)
		: FrameGraphNode(name, id), pass(std::move(pass))
	{}

	std::unique_ptr<RenderPassAbstract> pass;

	// Resources that were created by this pass
	std::vector<FrameGraphResource> creates;
	// Resources that needed read access to execute this pass
	std::vector<ResourceAccessDescription> reads;
	// Resources that needed write access to execute this pass
	std::vector<ResourceAccessDescription> writes;

	bool has_side_effect = false;
};

class ResourceNode final : public FrameGraphNode 
{
public:
	ResourceNode(const ResourceNode &) = delete;
	ResourceNode(ResourceNode &&) noexcept = default;

	ResourceNode &operator=(const ResourceNode &) = delete;
	ResourceNode &operator=(ResourceNode &&) = delete;

	FrameGraphResource getResource() const { return resource; }
	uint32_t getVersion() const { return version; }

private:
	friend class FrameGraph;

	ResourceNode(std::string name, uint32_t id, const FrameGraphResource &resource, uint32_t version)
		: FrameGraphNode(name, id), resource(resource), version(version)
	{}

	FrameGraphResource resource;
	uint32_t version;

	RenderPassNode *producer = nullptr;
};

// Holds any virtual resource
class ResourceEntry
{
	friend class FrameGraph;
public:
	ResourceEntry() = delete;
	ResourceEntry(const ResourceEntry &) = delete;
	ResourceEntry(ResourceEntry &&) noexcept = default;

	ResourceEntry &operator=(const ResourceEntry &) = delete;
	ResourceEntry &operator=(ResourceEntry &&) noexcept = delete;

	const int32_t resource_id;
	int version = 0;

	RenderPassNode *producer = nullptr;
	RenderPassNode *last_consumer = nullptr;

	template <typename T>
	auto *getModel() const
	{
		return static_cast<Model<T> *>(concept.get());
	}


	template <typename T>
	typename T &getResource() const
	{
		return getModel<T>()->resource;
	}

	template <typename T>
	const typename T::Description &getDescription() const
	{
		return getModel<T>()->desc;
	}

	bool isTransient() const { return is_transient; }

	void create()
	{
		assert(is_transient);
		concept->create();
	}

	void destroy()
	{
		assert(is_transient);
		concept->destroy();
	}

	void preRead(CommandBuffer &command_buffer, uint32_t flags)
	{
		concept->preRead(command_buffer, flags);
	}

	void preWrite(CommandBuffer &command_buffer, uint32_t flags)
	{
		concept->preWrite(command_buffer, flags);
	}
private:
	struct Concept
	{
		virtual ~Concept() = default;

		virtual void create() = 0;
		virtual void destroy() = 0;
		virtual void preRead(CommandBuffer &command_buffer, uint32_t flags) = 0;
		virtual void preWrite(CommandBuffer &command_buffer, uint32_t flags) = 0;
	};

	template <typename T>
	struct Model : Concept
	{
		Model(const typename T::Description &desc, T &&resource): desc(desc), resource(std::move(resource)) {}

		void create() override
		{
			resource.create(desc);
		}

		void destroy() override
		{
			resource.destroy(desc);
		}

		void preRead(CommandBuffer &command_buffer, uint32_t flags) override
		{
			resource.preRead(desc, command_buffer, flags);
		}

		void preWrite(CommandBuffer &command_buffer, uint32_t flags) override
		{
			resource.preWrite(desc, command_buffer, flags);
		}

		const typename T::Description desc;
		T resource;
	};

	template <typename T>
	ResourceEntry(uint32_t id, const typename T::Description &desc, T &&resource, bool transient) : resource_id(id), concept(std::make_unique<Model<T>>(desc, std::forward<T>(resource))), is_transient(transient)
	{}

	std::unique_ptr<Concept> concept;

	bool is_transient = false;
};

class FrameGraph
{
private:
	template <typename T>
	FrameGraphResource createResource(const std::string name, const typename T::Description &desc, T &&resource, bool transient)
	{
		uint32_t resource_id = resource_registry.size();
		resource_registry.emplace_back(ResourceEntry(resource_id, desc, std::forward<T>(resource), transient));

		auto &resource_node = createResourceNode(name, resource_id, 0);

		return resource_node.getId();
	}

public:
	template <typename Data, typename Setup, typename Execute>
	Data addCallbackPass(std::string name, Setup setup, Execute execute)
	{
		RenderPass<Data, Execute> *pass = new RenderPass<Data, Execute>(execute);
		int32_t node_id = renderpass_nodes.size();
		RenderPassNode &pass_node = renderpass_nodes.emplace_back(RenderPassNode(name, node_id, std::unique_ptr<RenderPass<Data, Execute>>(pass)));

		RenderPassBuilder builder(*this, pass_node);

		
		setup(builder, pass->data);
		return pass->data;
	}

	// Used for importing persistent resources
	template <typename T>
	FrameGraphResource importResource(const std::string name, const typename T::Description &desc, T &&resource)
	{
		return createResource<T>(name, desc, std::forward<T>(resource), false);
	}

	template <typename T>
	const typename T::Description &getDescription(FrameGraphResource resource_id)
	{
		return getResourceEntry(resource_id).getDescription<T>();
	}

	const FrameGraphBlackboard &getBlackboard() const { return blackboard; }
	FrameGraphBlackboard &getBlackboard() { return blackboard; }

	void compile();

	// Go through pases and execute them
	void execute(CommandBuffer &command_buffer);

private:
	friend class GraphViz;
	friend class RenderPassBuilder;
	friend class RenderPassResources;



	ResourceNode &createResourceNode(const std::string name, FrameGraphResource resource, int version)
	{
		int32_t node_id = resource_nodes.size();
		resource_nodes.emplace_back(ResourceNode(name, node_id, resource, version));
		return resource_nodes.back();
	}

	FrameGraphResource cloneResource(FrameGraphResource resource)
	{
		auto &entry = getResourceEntry(resource);
		entry.version++;

		auto &node = resource_nodes[resource];
		auto &cloned_node = createResourceNode(node.name, node.resource, entry.version);
		return cloned_node.id;
	}

	ResourceEntry &getResourceEntry(const ResourceNode &resource_node)
	{
		return resource_registry[resource_node.getResource()];
	}

	ResourceEntry &getResourceEntry(FrameGraphResource resource_id)
	{
		return resource_registry[resource_nodes[resource_id].getResource()];
	}


	std::vector<ResourceEntry> resource_registry;

	std::vector<RenderPassNode> renderpass_nodes;
	std::vector<ResourceNode> resource_nodes;

	FrameGraphBlackboard blackboard;
};


class RenderPassResources
{
public:
	template <typename T>
	T &getResource(FrameGraphResource resource) const
	{
		return frameGraph.getResourceEntry(resource).getResource<T>();
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

	void operator()(const RenderPassResources &resources, CommandBuffer &command_buffer) override
	{
		execute(data, resources, command_buffer);
	}

	Execute execute;
	Data data{};
};


// Used during setup phase, created by frame graph, creates one PassNode
class RenderPassBuilder
{
public:
	FrameGraphResource read(FrameGraphResource resource, uint32_t flags = 0)
	{
		return renderpass_node.reads.emplace_back(RenderPassNode::ResourceAccessDescription{resource, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
	}

	FrameGraphResource write(FrameGraphResource resource, uint32_t flags = 0)
	{
		//frameGraph.getResourceEntry(resource)

		if (renderpass_node.isCreating(resource))
		{
			return renderpass_node.writes.emplace_back(RenderPassNode::ResourceAccessDescription{resource, RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
		} else
		{
			// When writing to already created resource, we need to clone it and rename for better error handling
			renderpass_node.reads.emplace_back(RenderPassNode::ResourceAccessDescription{resource, RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG | flags}).resource;
			return renderpass_node.writes.emplace_back(RenderPassNode::ResourceAccessDescription{frameGraph.cloneResource(resource), RenderPassNode::RESOURCE_ACCESS_NO_FLAG | flags}).resource;
		}
	}

	template <typename T>
	FrameGraphResource createResource(std::string name, const typename T::Description &desc)
	{
		FrameGraphResource resource_id = frameGraph.createResource<T>(name, desc, T{}, true);
		renderpass_node.creates.emplace_back(resource_id);
		return resource_id;
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