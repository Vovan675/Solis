#pragma once
#include "FrameGraph.h"
#include "FrameGraphRHIResources.h"
#include "RHI/RHITexture.h"

static FrameGraphResource importTexture(FrameGraph &fg, RHITexture *t)
{
	PROFILE_CPU_FUNCTION();
	FrameGraphTexture::Description desc;
	const TextureDescription &src_desc = t->getDescription();
	memcpy(&desc, &src_desc, sizeof(TextureDescription));
	return fg.importResource<FrameGraphTexture>(t->getDebugName(), desc, FrameGraphTexture {t});
}