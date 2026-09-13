#pragma once
#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class DefferedLightingRenderer: public RendererBase
{
public:
	struct Constants
	{
		uint32_t albedo_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t shading_tex_id = 0;
		uint32_t light_index = 0;
		uint32_t ray_traced_visibility_tex_id = 0;
	} constants;

	DefferedLightingRenderer();
	virtual ~DefferedLightingRenderer();

	void renderLights(FrameGraph &fg, uint32_t lights_count);

public:
	Engine::Mesh *icosphere_mesh;
};
