#pragma once

#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Core/Variables.h"

class SkyRenderer: public RendererBase
{
public:
	SkyRenderer();
	~SkyRenderer() {}
	void addProceduralPasses(FrameGraph &fg);
	void addCompositePasses(FrameGraph &fg);

	bool isDirty() const { return is_dirty; }

	struct Uniforms
	{
		glm::vec3 sun_direction = glm::vec3(1, 0.7, 0);
		glm::mat4 mvp;
		float sky_luminance_scale = 1000.0f;
	} procedural_uniforms;

	RHITextureRef cube_texture;
private:
	void update_sun_from_scene();
	bool update_resources();

	RHIShaderRef vertex_shader;
	RHIShaderRef fragment_shader;
	RHIShaderRef vertex_procedural_shader;
	RHIShaderRef fragment_procedural_shader;

	Engine::Mesh *mesh;
	SkyMode created_mode = SKY_MODE_CUBEMAP;
	AssetReference created_hdri;

	Uniforms prev_uniform;

	glm::vec4 sun_illuminance = glm::vec4(100000.0f);

	bool is_dirty = false;
};

