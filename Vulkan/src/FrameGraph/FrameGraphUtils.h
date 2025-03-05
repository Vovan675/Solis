#pragma once
#include "FrameGraph.h"
#include "FrameGraphRHIResources.h"
#include "RHI/RHITexture.h"

static FrameGraphResource importTexture(FrameGraph &fg, std::shared_ptr<RHITexture> t)
{
	PROFILE_CPU_FUNCTION();
	FrameGraphTexture::Description desc;
	memcpy(&desc, &t->getDescription(), sizeof(TextureDescription));
	return fg.importResource<FrameGraphTexture>(t->getDebugName(), desc, FrameGraphTexture {t});
}