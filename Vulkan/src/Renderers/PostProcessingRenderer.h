#pragma once
#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class PostProcessingRenderer: public RendererBase
{
public:
	struct FilmUBO
	{
		uint32_t composite_final_tex_id = 0;
		float use_vignette = 1;
		float vignette_radius = 0.7;
		float vignette_smoothness = 0.2;
	} film_ubo;



	PostProcessingRenderer();
	virtual ~PostProcessingRenderer();

	void addPasses(FrameGraph &fg);

	void renderImgui();
private:
	void addFilmPass(FrameGraph &fg);
	void addFxaaPass(FrameGraph &fg);

	FrameGraphResource current_output;
};

