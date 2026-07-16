#pragma once
#include "RHI/RHIDefinitions.h"

class FrameGraph;

class UpscaleRenderer
{
public:
	glm::ivec2 getRenderResolution(glm::ivec2 output_resolution);
	void addPasses(FrameGraph &frame_graph);

	// From DLSS guide section 3.5
	static float getMipBias(float upscale_factor)
	{
		float epsilon = 0.0f;
		return glm::log2(upscale_factor) - 1.0f + epsilon;
	}

private:
	bool history_valid = false;
	glm::ivec2 last_render_resolution = glm::ivec2(0);
};
