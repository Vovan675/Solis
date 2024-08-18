#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"
#include "Scene/Scene.h"


class DefferedLightingRenderer: public RendererBase
{
public:
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 light_matrix;
	} ubo_sphere;

	struct UBO
	{
		uint32_t albedo_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t shading_tex_id = 0;
		uint32_t shadow_map_tex_id = 0;
	} ubo;

	std::shared_ptr<Texture> shadow_map_cubemap;

	// TODO: use uniform buffer, move to Renderer::setDefaultResources
	struct PushConstant
	{
		glm::vec4 light_pos;
		glm::vec4 light_color;
		float light_intensity;
		float light_range_square; // radius ^ 2
		float padding[2];
	} constants;

	DefferedLightingRenderer();
	virtual ~DefferedLightingRenderer();

	void renderLights(Scene *scene, CommandBuffer &command_buffer, uint32_t image_index);

	void renderImgui();
public:
	std::shared_ptr<Shader> lighting_vertex_shader;
	std::shared_ptr<Shader> lighting_fragment_shader;
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

