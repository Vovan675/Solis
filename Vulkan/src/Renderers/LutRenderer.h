#pragma once

#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "FrameGraph/FrameGraph.h"

class LutRenderer: public RendererBase
{
public:
	LutRenderer();
	void addPasses(FrameGraph &fg);

	std::shared_ptr<RHITexture> brdf_lut_texture;
};

