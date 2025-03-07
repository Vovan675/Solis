#pragma once
#include "FrameGraph.h"
#include "TransientResources.h"
#include "RHI/RHITexture.h"
#include "Rendering/Renderer.h"
#include "BindlessResources.h"

enum TextureResourceAccess: uint32_t
{
	// 1 << 0 reserved
	TEXTURE_RESOURCE_ACCESS_GENERAL = 1 << 1,
	TEXTURE_RESOURCE_ACCESS_TRANSFER = 1 << 2,
};

class FrameGraphTexture
{
public:
	struct Description
	{
		bool is_cube = false;
		uint32_t width;
		uint32_t height;
		uint32_t mip_levels = 1;
		uint32_t array_levels = 1;
		SampleCount sample_count = SAMPLE_COUNT_1;
		Format format;
		uint32_t usage_flags = 0;
		SamplerMode sampler_mode = SAMPLER_MODE_REPEAT;
		Filter filtering = FILTER_LINEAR;
		bool anisotropy = false;
		bool use_comparison_less = false;

		std::string debug_name;
	};
	RHITexture *texture;


	void create(const Description &desc)
	{
		TextureDescription d = (TextureDescription &)desc;
		texture = TransientResources::getTemporaryTexture(d);

		if (!texture->isValid())
		{
			texture->fill();
		}
		texture->setDebugName(desc.debug_name.c_str());
		gDynamicRHI->getBindlessResources()->addTexture(texture);

		if (strcmp(texture->getDebugName(), "SSAO Raw Image") == 0)
		{
			int i = 0;
		}
	}

	void destroy(const Description &desc)
	{
		if (strcmp(texture->getDebugName(), "SSAO Raw Image") == 0)
		{
			int i = 0;
		}

		TransientResources::releaseTemporaryTexture(texture);
		texture = nullptr;
	}

	void preRead(const Description &desc, RHICommandList *cmd_list, uint32_t flags)
	{
		TextureLayoutType new_layout;
		if (flags & TEXTURE_RESOURCE_ACCESS_GENERAL)
			new_layout = TEXTURE_LAYOUT_GENERAL;
		else if (flags & TEXTURE_RESOURCE_ACCESS_TRANSFER)
			new_layout = TEXTURE_LAYOUT_TRANSFER_SRC;
		else
			new_layout = TEXTURE_LAYOUT_SHADER_READ;
		texture->transitLayout(cmd_list, new_layout);
	}

	void preWrite(const Description &desc, RHICommandList *cmd_list, uint32_t flags)
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

	
	uint32_t addToBindless() const
	{
		return gDynamicRHI->getBindlessResources()->addTexture(texture);
	}

	uint32_t getBindlessId() const
	{
		return gDynamicRHI->getBindlessResources()->getTextureIndex(texture);
	}
};