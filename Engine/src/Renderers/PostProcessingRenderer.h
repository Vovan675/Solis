#pragma once
#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class PostProcessingRenderer: public RendererBase
{
public:
	struct FilmUBO
	{
		uint32_t composite_final_tex_id = 0;
		float use_vignette = 0;
		float vignette_radius = 0.7;
		float vignette_smoothness = 0.2;
		float exposure = 0.000013;
		int tonemapper_mode = 0;
	} film_ubo;

	PostProcessingRenderer();
	virtual ~PostProcessingRenderer();

	void addPasses(FrameGraph &fg);

private:
	void addFilmPass(FrameGraph &fg);
	void addFxaaPass(FrameGraph &fg);

	GraphicsResourceName last_output;
};

