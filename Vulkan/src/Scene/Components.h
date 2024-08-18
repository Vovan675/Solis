#pragma once
#include <entt/entt.hpp>
#include "Mesh.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "RHI/Texture.h"
#include "Material.h"

class Model;

struct TransformComponent
{
	std::string name = "";
	entt::entity parent = entt::null;
	std::vector<entt::entity> children;

	glm::vec3 position = glm::vec3(0, 0, 0);
	glm::vec3 scale = glm::vec3(1, 1, 1);
	
	glm::vec3 rotation_euler =glm::vec3(0, 0, 0);
	glm::quat rotation = glm::identity<glm::quat>();

	void setTransform(glm::mat4 transform)
	{
		glm::vec3 skew;
		glm::vec4 persp;
		glm::decompose(transform, scale, rotation, position, skew, persp);
		rotation_euler = glm::eulerAngles(rotation);
	}

	glm::mat4 getTransformMatrix() const
	{
		return glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
	}
};

struct MeshRendererComponent
{
	struct MeshId
	{
		Model* model;
		size_t mesh_id = 0;

		std::shared_ptr<Engine::Mesh> getMesh();
	};
	std::vector<MeshId> meshes;
	std::vector<std::shared_ptr<Material>> materials;
};

struct LightComponent
{
	LightComponent()
	{
		TextureDescription description;
		description.width = 4096;
		description.height = 4096;
		description.imageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		description.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		description.is_cube = true;
		description.mipLevels = 1;
		description.filtering = VK_FILTER_NEAREST;
		shadow_map = std::make_shared<Texture>(description);
		shadow_map->fill();
	}

	glm::vec3 color;
	float intensity;
	float radius;

	std::shared_ptr<Texture> shadow_map;
};