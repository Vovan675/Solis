#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class PrefilterRenderer: public RendererBase
{
public:
	struct PushConstantFrag
	{
		float roughness = 0;
	} constants_frag;

	PrefilterRenderer();

	void addPass(FrameGraph &fg);

	std::shared_ptr<Texture> prefilter_texture;

private:
	std::shared_ptr<Shader> compute_shader;
};

