#pragma once
#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

enum ExposureMode
{
	EXPOSURE_MODE_EV100 = 0,
	EXPOSURE_MODE_CAMERA,
};

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

	int exposure_mode = EXPOSURE_MODE_EV100;
	float aperture = 2.8f;
	float shutter_speed = 1.0f / 8192.0f;
	float iso = 100.0f;

	PostProcessingRenderer();
	virtual ~PostProcessingRenderer();

	void addPasses(FrameGraph &fg);

	void renderImgui();
private:
	void addFilmPass(FrameGraph &fg);
	void addFxaaPass(FrameGraph &fg);

	GraphicsResourceName last_output;
};

