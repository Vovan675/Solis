#pragma once
#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "Scene/Scene.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class DefferedLightingRenderer: public RendererBase
{
public:
	struct UniformBufferObject
	{
		glm::mat4 model;
		std::array<glm::mat4, 4> light_matrix;
		glm::vec4 cascade_splits;
	} ubo_sphere;

	struct UBO
	{
		uint32_t albedo_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t shading_tex_id = 0;
		uint32_t shadow_map_tex_id = 0;
	} ubo;

	// TODO: use uniform buffer, move to Renderer::setDefaultResources
	struct PushConstant
	{
		glm::vec4 light_pos;
		glm::vec4 light_color;
		float light_intensity;
		float light_range_square; // radius ^ 2
		float z_far;
	} constants;

	DefferedLightingRenderer();
	virtual ~DefferedLightingRenderer();

	void renderLights(FrameGraph &fg);

	void renderImgui();
public:
	std::shared_ptr<Engine::Mesh> icosphere_mesh;

	struct LightData
	{
		glm::vec4 position;
		glm::vec4 color;
		float intensity;
		float radius;
	};
	std::vector<LightData> lights;

	glm::vec3 position = glm::vec3(0, 0, 0);
	float radius = 1.0;
	float intensity = 1.0;
};

