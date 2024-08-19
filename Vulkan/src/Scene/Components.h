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

enum LIGHT_TYPE
{
	LIGHT_TYPE_POINT,
	LIGHT_TYPE_DIRECTIONAL,
};

#define SHADOW_MAP_CASCADE_COUNT 4
struct LightComponent
{
	LightComponent()
	{
		recreateTexture();
	}

	LIGHT_TYPE getType() const { return type; };
	void setType(LIGHT_TYPE type)
	{
		if (this->type == type)
			return;
		this->type = type;
		recreateTexture();
	}

	void recreateTexture()
	{
		if (type == LIGHT_TYPE_POINT)
		{
			TextureDescription description;
			description.width = shadow_map_size;
			description.height = shadow_map_size;
			description.imageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			description.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
			description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			description.is_cube = true;
			description.mipLevels = 1;
			description.filtering = VK_FILTER_NEAREST;
			shadow_map = std::make_shared<Texture>(description);
			shadow_map->fill();
		} else if (type == LIGHT_TYPE_DIRECTIONAL)
		{
			TextureDescription description;
			description.width = shadow_map_size;
			description.height = shadow_map_size;
			description.imageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			description.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
			description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			description.is_cube = false;
			description.mipLevels = 1;
			description.arrayLevels = 4;
			description.filtering = VK_FILTER_NEAREST;
			description.sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			shadow_map = std::make_shared<Texture>(description);
			shadow_map->fill();
		}
	}

	glm::vec3 color;
	float intensity;
	float radius;

	float shadow_map_size = 4096;

	std::shared_ptr<Texture> shadow_map;

private:
	LIGHT_TYPE type = LIGHT_TYPE_POINT;
	friend class Application;
	friend class DefferedLightingRenderer;
	struct CascadeData
	{
		glm::mat4 viewProjMatrix;
		glm::mat4 View;
		float splitDepth;
	};
	std::array<CascadeData, SHADOW_MAP_CASCADE_COUNT> cascades;
};