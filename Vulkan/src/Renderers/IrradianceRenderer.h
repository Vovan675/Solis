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
	
	std::shared_ptr<RHITexture> irradiance_texture;

private:
	std::shared_ptr<RHIShader> compute_shader;
};

