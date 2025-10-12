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
		uint32_t samples_count;
	} constants_frag;

	PrefilterRenderer();

	void addPass(FrameGraph &fg, uint32_t samples_count);

	RHITextureRef prefilter_texture;

private:
	RHIShaderRef compute_shader;
};

