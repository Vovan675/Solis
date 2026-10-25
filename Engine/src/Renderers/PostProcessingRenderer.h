#pragma once
#include "Rendering/GlobalPipeline.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class PostProcessingRenderer
{
public:
	struct FilmUBO
	{
		uint32_t composite_final_tex_id = 0;
		float use_vignette = 0;
		float vignette_radius = 0.7;
		float vignette_smoothness = 0.2;
		float exposure = 0.000013;
		uint32_t tonemap_lut_tex_id = 0;
		uint32_t bloom_tex_id = 0;
		float bloom_intensity = 0;
	} film_ubo;

	void addPasses(FrameGraph &fg);

private:
	void addTonemapLutPass(FrameGraph &fg);
	void addBloomPass(FrameGraph &fg);
	void addFilmPass(FrameGraph &fg);
	void addFxaaPass(FrameGraph &fg);

	GraphicsResourceName last_output;
};

