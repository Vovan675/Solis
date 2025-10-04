#pragma once

#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraph.h"

class LutRenderer: public RendererBase
{
public:
	LutRenderer();
	void addPasses(FrameGraph &fg);

	RHITextureRef brdf_lut_texture;
};

