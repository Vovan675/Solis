#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class SSRRenderer: public RendererBase
{
public:
	struct UBO
	{
		uint32_t composite_final_tex_id = 0;
		float use_vignette = 1;
		float vignette_radius = 0.7;
		float vignette_smoothness = 0.2;
	} ubo;

	void addPasses(FrameGraph &fg);

private:
};

