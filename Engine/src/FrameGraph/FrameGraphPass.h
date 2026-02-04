#pragma once
#include "FrameGraphRHIResources.h"
#include "RHI/DynamicRHI.h"

class RenderPassResources;
struct RenderPassAbstract
{
	virtual ~RenderPassAbstract() = default;
	virtual void operator()(const RenderPassResources &resources, RHICommandList *cmd_list) = 0;
};

class RenderPassNode final
{
public:
	RenderPassNode() = delete;
	RenderPassNode(const RenderPassNode &) = delete;
	RenderPassNode(RenderPassNode &&) noexcept = default;
	virtual ~RenderPassNode() = default;

	RenderPassNode &operator=(const RenderPassNode &) = delete;
	RenderPassNode &operator=(RenderPassNode &&) = delete;

	bool isReading(FrameGraphTextureId resource) const
	{
		return eastl::find(texture_reads.cbegin(), texture_reads.cend(), resource) != texture_reads.cend();
	}

	bool isWriting(FrameGraphTextureId resource) const
	{
		return eastl::find(texture_writes.cbegin(), texture_writes.cend(), resource) != texture_writes.cend();
	}

	bool isCreating(FrameGraphTextureId resource) const
	{
		return eastl::find(texture_creates.cbegin(), texture_creates.cend(), resource) != texture_creates.cend();
	}

	bool isCreating(FrameGraphBufferId resource) const
	{
		return eastl::find(buffer_creates.cbegin(), buffer_creates.cend(), resource) != buffer_creates.cend();
	}

	const auto &getTextureCreates() const { return texture_creates; }
	const auto &getTextureReads() const { return texture_reads; }
	const auto &getTextureWrites() const { return texture_writes; }

	const auto &getBufferCreates() const { return buffer_creates; }
	const auto &getBufferReads() const { return buffer_reads; }
	const auto &getBufferWrites() const { return buffer_writes; }

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
		texture_creates.reserve(8);
		texture_reads.reserve(16);
		texture_writes.reserve(8);

		buffer_creates.reserve(8);
		buffer_reads.reserve(16);
		buffer_writes.reserve(8);
	}

	std::unique_ptr<RenderPassAbstract> pass; // TODO: this is error (leak memory)

	// Resources that were created by this pass
	eastl::vector<FrameGraphTextureId> texture_creates;
	// Resources that needed read access to execute this pass
	eastl::vector<FrameGraphTextureId> texture_reads;
	// Resources that needed write access to execute this pass
	eastl::vector<FrameGraphTextureId> texture_writes;
	eastl::unordered_map<FrameGraphTextureId, ResourceState, FrameGraphResourceIdHash> texture_usage;

	// Resources that were created by this pass
	eastl::vector<FrameGraphBufferId> buffer_creates;
	// Resources that needed read access to execute this pass
	eastl::vector<FrameGraphBufferId> buffer_reads;
	// Resources that needed write access to execute this pass
	eastl::vector<FrameGraphBufferId> buffer_writes;
	eastl::unordered_map<FrameGraphBufferId, ResourceState, FrameGraphResourceIdHash> buffer_usage;

	bool has_side_effect = false;
};