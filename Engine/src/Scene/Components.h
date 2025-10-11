#pragma once
#include <entt/entt.hpp>
#include "Core/Core.h"
#include "Rendering/Mesh.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "RHI/RHITexture.h"
#include "Rendering/Material.h"
#include "Scene.h"

struct MeshNode;
class Model;

struct TransformComponent
{
private:
	glm::vec3 local_position = glm::vec3(0, 0, 0);
	glm::vec3 local_scale = glm::vec3(1, 1, 1);
	
	glm::vec3 local_rotation_euler = glm::vec3(0, 0, 0);
	glm::quat local_rotation = glm::identity<glm::quat>();

	glm::mat4 world_transform = glm::mat4(1);
	glm::mat4 inverse_world_transform = glm::mat4(1);
public:
	eastl::string name = "";
	entt::entity owner;

	entt::entity parent = entt::null;
	eastl::vector<entt::entity> children;

	void setLocalTransform(glm::mat4 transform)
	{
		glm::vec3 skew;
		glm::vec4 persp;
		glm::decompose(transform, local_scale, local_rotation, local_position, skew, persp);
		local_rotation_euler = glm::eulerAngles(local_rotation);
		Scene::propagate_local_transforms_update(owner);
	}

	glm::vec3 getLocalPosition() const { return local_position; }
	void setPosition(glm::vec3 new_position)
	{
		local_position = new_position;
		Scene::propagate_local_transforms_update(owner);
	}

	glm::vec3 getLocalScale() const { return local_scale; }
	void setLocalScale(glm::vec3 new_scale)
	{
		local_scale = new_scale;
		Scene::propagate_local_transforms_update(owner);
	}

	glm::quat getLocalRotation() const { return local_rotation; }
	void setLocalRotation(glm::quat rot)
	{
		local_rotation_euler = glm::eulerAngles(rot);
		local_rotation = rot;
		Scene::propagate_local_transforms_update(owner);
	}

	glm::vec3 getLocalRotationEuler() const { return local_rotation_euler; }
	void setLocalRotationEuler(glm::vec3 rot)
	{
		local_rotation_euler = rot;
		local_rotation = glm::quat(rot);
		Scene::propagate_local_transforms_update(owner);
	}

	glm::mat4 getLocalTransform() const
	{
		return glm::translate(glm::mat4(1.0f), local_position) * glm::toMat4(local_rotation) * glm::scale(glm::mat4(1.0f), local_scale);
	}

	void setWorldTransform(glm::mat4 transform)
	{
		world_transform = transform;
		Scene::propagate_world_transforms_update(owner);
	}

	const glm::mat4 &getWorldTransform() const
	{
		return world_transform;
	}

	const glm::mat4 &getInverseWorldTransform() const
	{
		return inverse_world_transform;
	}
protected:
	friend class Scene;
};

struct MeshRendererComponent
{
	struct MeshId
	{
		Model *model;
		size_t mesh_id = 0;

		Engine::Mesh *getMesh();
	};
	eastl::vector<MeshId> meshes;
	eastl::vector<Ref<Material>> materials;

	void setFromMeshNode(Ref<Model> model, MeshNode *mesh_node);
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
	~LightComponent()
	{
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

	RHITextureRef shadow_map = nullptr;

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
	eastl::array<CascadeData, SHADOW_MAP_CASCADE_COUNT> cascades;
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