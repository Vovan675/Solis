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

	RHITextureRef brdf_lut_texture;
};

