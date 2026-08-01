#pragma once
#include <filesystem>
#include "RHI/RHITexture.h"

struct EditorContext;
class SkyRenderer;
class PostProcessingRenderer;

class MitsubaBridge
{
public:
	void renderImGui(EditorContext &context);
	void runRender(EditorContext &context);

	SkyRenderer *sky_renderer = nullptr;
	PostProcessingRenderer *post_renderer = nullptr;

private:
	bool export_scene(EditorContext &context, const std::filesystem::path &scene_path);
	bool launch_mitsuba(const std::filesystem::path &scene_path, const std::filesystem::path &output_path);
	std::filesystem::path get_mitsuba_path();

	RHITextureRef result_texture;
	eastl::string status;

	float render_scale = 0.5f;
};
