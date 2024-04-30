#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"

// Basic material
struct Material
{
	uint32_t albedo_tex_id = 0;
	glm::vec4 albedo = glm::vec4(1, 1, 1, 1);
	float use_albedo_map = 0;

	uint32_t rougness_tex_id = 0;
	float use_rougness_map = 0;
	float metalness = 0;
	float roughness = 0;
	float specular = 0.5f;
};

class MeshRenderer : public RendererBase
{
public:
	MeshRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Engine::Mesh> mesh);
	virtual ~MeshRenderer();

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	void setPosition(glm::vec3 pos) { position = pos; }
	void setRotation(glm::quat rot) { rotation = rot; }
	void setScale(glm::vec3 scale) { this->scale = scale; }
	void setMaterial(Material mat) { this->mat = mat; }
	void updateUniformBuffer(uint32_t image_index) override;

private:
	// TODO: move to Renderer::setDefaultResources
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
		alignas(16) glm::vec3 camera_position;
	};

	struct PushConstant
	{
		alignas(16) glm::mat4 model;
	};

	DescriptorLayout descriptor_layout;

	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	std::vector<VkDescriptorSet> descriptor_sets;
	std::vector<std::shared_ptr<Buffer>> uniform_buffers;
	std::vector<void *> uniform_buffers_mapped;

	std::vector<std::shared_ptr<Buffer>> material_uniform_buffers;
	std::vector<void *> material_uniform_buffers_mapped;

	std::shared_ptr<Texture> texture;
	std::shared_ptr<Engine::Mesh> mesh;

	std::shared_ptr<Camera> camera;
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.0f);
	Material mat {};
};

