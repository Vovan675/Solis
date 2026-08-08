#pragma once

#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphUtils.h"

enum SKY_MODE
{
	SKY_MODE_CUBEMAP,
	SKY_MODE_PROCEDURAL,
};

class SkyRenderer: public RendererBase
{
public:
	SkyRenderer();
	~SkyRenderer() {}
	void addProceduralPasses(FrameGraph &fg);
	void addCompositePasses(FrameGraph &fg);
	void renderImgui();

	void setMode(SKY_MODE mode);
	SKY_MODE getMode() const { return mode; }

	const eastl::string &getEnvironmentPath() const { return environment_path; }

	bool isDirty();

	struct Uniforms
	{
		glm::vec3 sun_direction = glm::vec3(1, 0.7, 0);
		glm::mat4 mvp;
		float sky_luminance_scale = 1000.0f;
	} procedural_uniforms;

	glm::vec4 sun_illuminance = glm::vec4(100000.0f);

	float getSkyIntensity() const { return mode == SKY_MODE_CUBEMAP ? sky_intensity : 1.0f; }

	RHITextureRef cube_texture;
private:
	void create_mode_resources();

	float sky_intensity = 15000.0f;

	RHIShaderRef vertex_shader;
	RHIShaderRef fragment_shader;
	RHIShaderRef vertex_procedural_shader;
	RHIShaderRef fragment_procedural_shader;

	Engine::Mesh *mesh;
	SKY_MODE mode;
	eastl::string environment_path = "assets/kloppenheim_06_puresky_4k.hdr";

	Uniforms prev_uniform;

	bool is_force_dirty = false;
};

