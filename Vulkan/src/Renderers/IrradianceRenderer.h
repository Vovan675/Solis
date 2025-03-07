#pragma once

#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class IrradianceRenderer: public RendererBase
{
public:
	IrradianceRenderer();

	void addPass(FrameGraph &fg);
	
	RHITextureRef irradiance_texture;

private:
	RHIShaderRef compute_shader;
};

