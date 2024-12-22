#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class IrradianceRenderer: public RendererBase
{
public:
	IrradianceRenderer();

	void addPass(FrameGraph &fg);
	
	std::shared_ptr<Texture> irradiance_texture;

private:
	std::shared_ptr<Shader> compute_shader;
};

