#pragma once
#include "SSAORenderer.h"
#include "GTAORenderer.h"
#include "RTAORenderer.h"

struct AmbientOcclusionGuides
{
	int resolution_scale = 1;
	glm::ivec2 resolution{};
	GraphicsResourceName view_depth;
	GraphicsResourceName normal;
};

class AmbientOcclusionRenderer
{
public:
	void addPasses(FrameGraph &fg, const Camera &camera);

private:
	void add_guides_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, bool view_depth_mips);
	void add_temporal_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName input, GraphicsResourceName history, GraphicsResourceName accumulated, GraphicsResourceName output, int max_history_length, AmbientOcclusionAntilag antilag, float antilag_window);
	void add_spatial_filter_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName input, GraphicsResourceName output);
	void add_upsample_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName input, GraphicsResourceName output);

	SSAORenderer ssao_renderer;
	GTAORenderer gtao_renderer;
	RTAORenderer rtao_renderer;

	uint64_t last_temporal_frame = 0;
	glm::ivec2 last_temporal_resolution = glm::ivec2(0);
};
