#pragma once
#include "RHI/RHITexture.h"
#include "Rendering/Renderer.h"
#include "RHI/BindlessResources.h"
#include "Utils/Hashing.h"

enum class FrameGraphResourceType
{
	TEXTURE,
	BUFFER
};

struct GraphicsResourceName
{
	GraphicsResourceName(): name(nullptr) {}

	constexpr explicit GraphicsResourceName(char *name, uint32_t hash) : name(name), hashed_name(hash)
	{}

	operator const char *() const
	{
		return name;
	}

	bool operator==(const GraphicsResourceName &other) const
	{
		return hashed_name == other.hashed_name;
	}

	char *name;
	uint32_t hashed_name = 0;
};

namespace eastl
{
	template<>
	struct hash<GraphicsResourceName>
	{
		size_t operator()(const GraphicsResourceName &v) const
		{
			return v.hashed_name;
		}
	};
}

#define GFXRID(name) GraphicsResourceName(#name, crc::crc32(#name))
#define GFXRID_ID(name, id) GraphicsResourceName(#name, crc::crc32(#name) + (id))

class RenderPassNode;

template <FrameGraphResourceType ResourceType>
struct FrameGraphResource
{
	eastl::string name;
	bool is_transient = false;

	int32_t resource_id;
	int version = 0;
	int ref_count = 0;

	RenderPassNode *producer = nullptr;
	RenderPassNode *last_consumer = nullptr;
};

struct FrameGraphTexture : public FrameGraphResource<FrameGraphResourceType::TEXTURE>
{
	RHITextureRef resource;
	TextureDescription desc;

	// From description (transient resource)
	FrameGraphTexture(uint32_t id, const TextureDescription &desc, eastl::string name) : desc(desc)
	{
		resource_id = id;
		is_transient = true;
		this->name = name;
	}

	// From already created resource (imported resource)
	FrameGraphTexture(uint32_t id, RHITexture *resource, eastl::string name) : resource(resource), desc(resource->getDescription())
	{
		resource_id = id;
		is_transient = false;
		this->name = name;
	}

	FrameGraphTexture(FrameGraphTexture &&) noexcept = default;

	void transition(RHICommandList *cmd_list, ResourceState usage)
	{
		TextureLayoutType new_layout = TEXTURE_LAYOUT_SHADER_READ;
		if (usage == ResourceState::UAV)
			new_layout = TEXTURE_LAYOUT_UAV;
		else if (usage == ResourceState::RENDER_TARGET)
			new_layout = TEXTURE_LAYOUT_ATTACHMENT;
		else if (usage == ResourceState::DEPTH_READ)
			new_layout = TEXTURE_LAYOUT_DEPTH_READ;
		else if (usage == ResourceState::COPY_SRC)
			new_layout = TEXTURE_LAYOUT_TRANSFER_SRC;
		else if (usage == ResourceState::COPY_DST)
			new_layout = TEXTURE_LAYOUT_TRANSFER_DST;
		resource->transitLayout(cmd_list, new_layout);
	}

	eastl::string toString() const
	{
		std::ostringstream out;
		if (desc.depth > 1)
			out << desc.width << "x" << desc.height << "x" << desc.depth << " (" << getFormatName(desc.format) << ")";
		else
			out << desc.width << "x" << desc.height << " (" << getFormatName(desc.format) << ")";
		return out.str().c_str();
	}
};

struct FrameGraphBuffer : public FrameGraphResource<FrameGraphResourceType::BUFFER>
{
	RHIBufferRef resource;
	BufferDescription desc;

	// From description (transient resource)
	FrameGraphBuffer(uint32_t id, const BufferDescription &desc, eastl::string name) : desc(desc)
	{
		resource_id = id;
		is_transient = true;
		this->name = name;
	}

	// From already created resource (imported resource)
	FrameGraphBuffer(uint32_t id, RHIBuffer *resource, eastl::string name) : resource(resource), desc(resource->getDescription())
	{
		resource_id = id;
		is_transient = false;
		this->name = name;
	}

	FrameGraphBuffer(FrameGraphBuffer &&) noexcept = default;

	eastl::string toString() const
	{
		std::ostringstream out;
		out << "Size: " << desc.size;
		return out.str().c_str();
	}
};

// Just a handle to any resource
template <FrameGraphResourceType ResourceType>
struct FrameGraphResourceId
{
	inline const static uint32_t invalid_id = uint32_t(-1);

	FrameGraphResourceId(): id(invalid_id) {}
	FrameGraphResourceId(uint32_t id): id(id) {}

	bool isValid() const { return id != invalid_id; }
	bool operator==(const FrameGraphResourceId &other) const { return other.id == id; }

	uint32_t id;
};

using FrameGraphTextureId = FrameGraphResourceId<FrameGraphResourceType::TEXTURE>;
using FrameGraphBufferId = FrameGraphResourceId<FrameGraphResourceType::BUFFER>;

struct FrameGraphResourceIdHash
{
	size_t operator()(const FrameGraphTextureId &x) const { return x.id; }
	size_t operator()(const FrameGraphBufferId &x) const { return x.id; }
};