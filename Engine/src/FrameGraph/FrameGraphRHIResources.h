#pragma once
#include "TransientResources.h"
#include "RHI/RHITexture.h"
#include "Rendering/Renderer.h"
#include "RHI/BindlessResources.h"
#include "Utils/Hashing.h"

struct GraphicsResourceName
{
	GraphicsResourceName(): name(nullptr) {}

	explicit GraphicsResourceName(const char *name): name(name)
	{}

	operator const char *() const
	{
		return name;
	}

	const char *name;
};

namespace eastl
{
	template<>
	struct hash<GraphicsResourceName>
	{
		size_t operator()(const GraphicsResourceName &v) const
		{
			return crc::crc32((uint8_t *)v.name, strlen(v));
		}
	};
}

#define GFXRID(name) GraphicsResourceName(#name)

enum TextureResourceAccess: uint32_t
{
	// 1 << 0 reserved
	TEXTURE_RESOURCE_ACCESS_GENERAL = 1 << 1,
	TEXTURE_RESOURCE_ACCESS_TRANSFER = 1 << 2,
	TEXTURE_RESOURCE_ACCESS_READ_ONLY_DEPTH = 1 << 3,
};

class RenderPassNode;

class FrameGraphTexture
{
public:
	RHITexture *texture;
	eastl::string name;
	TextureDescription desc;

	bool is_transient = false;

	const int32_t resource_id;
	int version = 0;

	int ref_count = 0;
	RenderPassNode *producer = nullptr;
	RenderPassNode *last_consumer = nullptr;

	// From description (transient resource)
	FrameGraphTexture(uint32_t id, const TextureDescription &desc, eastl::string name) : resource_id(id), desc(desc), is_transient(true), name(name)
	{}

	// From already created texture (imported resource)
	FrameGraphTexture(uint32_t id, RHITexture *texture, eastl::string name) : resource_id(id), texture(texture), desc(texture->getDescription()), is_transient(false), name(name)
	{}

	FrameGraphTexture(FrameGraphTexture &&) noexcept = default;

	void create()
	{
		texture = TransientResources::getTemporaryTexture(desc);

		if (!texture->isValid())
			texture->fill();
		texture->setDebugName(name.c_str());
	}

	void destroy()
	{
		TransientResources::releaseTemporaryTexture(texture);
		texture = nullptr;
	}

	void preRead(RHICommandList *cmd_list, uint32_t flags)
	{
		TextureLayoutType new_layout;
		if (flags & TEXTURE_RESOURCE_ACCESS_GENERAL)
			new_layout = TEXTURE_LAYOUT_GENERAL;
		else if (flags & TEXTURE_RESOURCE_ACCESS_TRANSFER)
			new_layout = TEXTURE_LAYOUT_TRANSFER_SRC;
		else if (flags & TEXTURE_RESOURCE_ACCESS_READ_ONLY_DEPTH)
			new_layout = TEXTURE_LAYOUT_DEPTH_READ;
		else
			new_layout = TEXTURE_LAYOUT_SHADER_READ;
		texture->transitLayout(cmd_list, new_layout);
	}

	void preWrite(RHICommandList *cmd_list, uint32_t flags)
	{
		TextureLayoutType new_layout;
		if (flags & TEXTURE_RESOURCE_ACCESS_GENERAL)
			new_layout = TEXTURE_LAYOUT_GENERAL;
		else if (flags & TEXTURE_RESOURCE_ACCESS_TRANSFER)
			new_layout = TEXTURE_LAYOUT_TRANSFER_DST;
		else
			new_layout = TEXTURE_LAYOUT_ATTACHMENT;
		texture->transitLayout(cmd_list, new_layout);
	}

	uint32_t getBindlessId() const
	{
		return texture->getShaderResourceView()->getBindlessIndex();
	}

	eastl::string toString() const
	{
		std::ostringstream out;
		out << desc.width << "x" << desc.height << " (" << getFormatName(desc.format) << ")";
		return out.str().c_str();
	}
};