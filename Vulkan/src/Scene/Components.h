#pragma once
#include <entt/entt.hpp>
#include "Mesh.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "RHI/RHITexture.h"
#include "Material.h"

struct MeshNode;
class Model;

struct TransformComponent
{
	std::string name = "";
	entt::entity parent = entt::null;
	std::vector<entt::entity> children;

	glm::vec3 position = glm::vec3(0, 0, 0);
	glm::vec3 scale = glm::vec3(1, 1, 1);
	
	glm::vec3 rotation_euler = glm::vec3(0, 0, 0);
	glm::quat rotation = glm::identity<glm::quat>();

	void setTransform(glm::mat4 transform)
	{
		glm::vec3 skew;
		glm::vec4 persp;
		glm::decompose(transform, scale, rotation, position, skew, persp);
		rotation_euler = glm::eulerAngles(rotation);
	}

	void setRotation(glm::quat rot)
	{
		rotation_euler = glm::eulerAngles(rot);
		rotation = rot;
	}

	void setRotationEuler(glm::vec3 rot)
	{
		rotation_euler = rot;
		rotation = glm::quat(rot);
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
		std::shared_ptr<Model> model;
		size_t mesh_id = 0;

		std::shared_ptr<Engine::Mesh> getMesh();
	};
	std::vector<MeshId> meshes;
	std::vector<std::shared_ptr<Material>> materials;

	void setFromMeshNode(std::shared_ptr<Model> model, MeshNode *mesh_node);
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
			description.format = FORMAT_D32S8;
			description.usage_flags = TEXTURE_USAGE_ATTACHMENT;
			description.is_cube = true;
			description.mip_levels = 1;
			description.filtering = FILTER_NEAREST;
			shadow_map = gDynamicRHI->createTexture(description);
			shadow_map->fill();
			shadow_map->setDebugName("Cube Shadow Map");
		} else if (type == LIGHT_TYPE_DIRECTIONAL)
		{
			TextureDescription description;
			description.width = shadow_map_size;
			description.height = shadow_map_size;
			description.format = FORMAT_D32S8;
			description.usage_flags = TEXTURE_USAGE_ATTACHMENT;
			description.is_cube = false;
			description.mip_levels = 1;
			description.array_levels = 4;
			description.filtering = FILTER_LINEAR;
			description.sampler_mode = SAMPLER_MODE_CLAMP_TO_EDGE;
			description.use_comparison_less = true;
			shadow_map = gDynamicRHI->createTexture(description);
			shadow_map->fill();
			shadow_map->setDebugName("Cascaded Shadow Map");
		}
	}

	glm::vec3 color = glm::vec3(1, 1, 1);
	float intensity = 1.0f;
	float radius = 1.0f;

	float shadow_map_size = 4096;

	std::shared_ptr<RHITexture> shadow_map;

private:
	LIGHT_TYPE type = LIGHT_TYPE_POINT;
	friend class EditorApplication;
	friend class DefferedLightingRenderer;
	friend class ShadowRenderer;
	struct CascadeData
	{
		glm::mat4 viewProjMatrix;
		glm::mat4 View;
		float splitDepth;
	};
	std::array<CascadeData, SHADOW_MAP_CASCADE_COUNT> cascades;
};


// Physics

struct RigidBodyComponent
{
	bool is_static = false;

	float mass = 1.0f;
	float linear_damping = 0.01f;
	float angular_damping = 0.05f;
	bool gravity = true;
	bool is_kinematic = false;
};

struct BoxColliderComponent
{
	glm::vec3 half_extent = {0.5f, 0.5f, 0.5f};
};