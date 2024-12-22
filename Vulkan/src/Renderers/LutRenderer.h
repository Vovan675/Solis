#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "FrameGraph/FrameGraph.h"

class LutRenderer: public RendererBase
{
public:
	LutRenderer();
	void addPasses(FrameGraph &fg);

	std::shared_ptr<Texture> brdf_lut_texture;
};

