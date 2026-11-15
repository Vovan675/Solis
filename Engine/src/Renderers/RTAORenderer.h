#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Utils/Camera.h"

struct AmbientOcclusionGuides;

class RTAORenderer
{
public:
	RTAORenderer();

	void addPasses(FrameGraph &fg, const Camera &camera, const AmbientOcclusionGuides &guides, GraphicsResourceName output);

private:
	RHITextureRef sampling_noise;
	RHITextureRef temporal_sampling_noise;

	uint32_t accumulated_frames = 0;
	bool last_accumulate = false;
	glm::mat4 last_view_projection = glm::mat4(0.0f);
	glm::ivec2 last_resolution = glm::ivec2(0);
	float last_radius = 0.0f;
	float last_falloff = 0.0f;
	int last_rays_per_pixel = 0;
};
