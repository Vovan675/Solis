#pragma once
#include "FrameGraph/FrameGraph.h"

struct AmbientOcclusionGuides;

class GTAORenderer
{
public:
	GTAORenderer();

	void addPasses(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName output);

private:
	RHITextureRef sampling_noise;
	RHITextureRef temporal_sampling_noise;
};
