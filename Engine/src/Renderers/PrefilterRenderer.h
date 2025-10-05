#pragma once

#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class PrefilterRenderer: public RendererBase
{
public:
	struct PushConstantFrag
	{
		uint32_t input_tex_id = 0;
		float roughness = 0;
		uint32_t mip_count;
	} constants_frag;

	PrefilterRenderer();

	void addPass(FrameGraph &fg);

	RHITextureRef prefilter_texture;

private:
	RHIShaderRef compute_shader;
};

