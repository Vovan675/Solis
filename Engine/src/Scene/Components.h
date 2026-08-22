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
	glm::mat4 old_world_transform = glm::mat4(1);
	bool old_world_transform_set = false;
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

	glm::vec3 getLocalDirection(glm::vec3 direction)
	{
		glm::vec3 scale, position, skew;
		glm::vec4 persp;
		glm::quat rotation;
		glm::decompose(world_transform, scale, rotation, position, skew, persp);
		return normalize(rotation * direction);
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

	const glm::mat4 &getOldWorldTransform() const
	{
		return old_world_transform;
	}
protected:
	friend class Scene;
	friend struct Reflected<TransformComponent>;
};

REFLECT_BEGIN(TransformComponent)
	REFLECT_FIELD(name),
	REFLECT_FIELD(local_position),
	REFLECT_FIELD(local_rotation),
	REFLECT_FIELD(local_rotation_euler),
	REFLECT_FIELD(local_scale),
	REFLECT_FIELD(parent),
	REFLECT_FIELD(children),
REFLECT_END()

struct MeshRendererComponent
{
	struct MeshId
	{
		AssetReference model_asset;
		size_t mesh_id = 0;

		Model *getModel();
		Engine::Mesh *getMesh();

	private:
		Model *model = nullptr;
	};

	struct MaterialSlot
	{
		AssetReference material_asset;

		Material *getMaterial();
		void setMaterial(Ref<Material> mat);

	private:
		Ref<Material> material;
	};

	eastl::vector<MeshId> meshes;
	eastl::vector<MaterialSlot> materials;

	Material *getMaterial(int index);
	void setMaterial(int index, Ref<Material> material);
	void setFromMeshNode(Ref<Model> model, MeshNode *mesh_node);
};

REFLECT_BEGIN(MeshRendererComponent::MeshId)
	REFLECT_FIELD(model_asset).asset<Model>(),
	REFLECT_FIELD(mesh_id),
REFLECT_END()

REFLECT_BEGIN(MeshRendererComponent::MaterialSlot)
	REFLECT_FIELD(material_asset).asset<Material>(),
REFLECT_END()

REFLECT_BEGIN(MeshRendererComponent)
	REFLECT_FIELD(meshes),
	REFLECT_FIELD(materials),
REFLECT_END()

enum LIGHT_TYPE
{
	LIGHT_TYPE_POINT,
	LIGHT_TYPE_DIRECTIONAL,
};

#define SHADOW_MAP_CASCADE_COUNT 4
#define POINT_SHADOW_Z_NEAR 0.05f

inline const char *const light_type_items[] = {"Point", "Directional"};

struct LightComponent
{
	LIGHT_TYPE getType() const { return type; };
	void setType(LIGHT_TYPE new_type) { type = new_type; }

	RHITextureRef getShadowMap()
	{
		if (!shadow_map || created_type != type || created_shadow_map_size != shadow_map_size)
			recreateTexture();
		return shadow_map;
	}

	glm::vec3 getPhotometricIntensity() const;

	void recreateTexture()
	{
		created_type = type;
		created_shadow_map_size = shadow_map_size;

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
			description.depth_clear_value = 1.0f;
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
			description.depth_clear_value = 1.0f;
			shadow_map = gDynamicRHI->createTexture(description);
			shadow_map->fill();
			shadow_map->setDebugName("Cascaded Shadow Map");
		}
	}

	glm::vec3 color = glm::vec3(1, 1, 1);
	// Lux for directional, lumens for point
	float intensity = 1.0f;
	float attenuation_radius = 1.0f;

	int shadow_map_size = 2048;

	glm::mat4 getCascadeViewProj(int cascade) { return cascades[cascade].viewProjMatrix; }

private:
	LIGHT_TYPE type = LIGHT_TYPE_POINT;
	LIGHT_TYPE created_type = LIGHT_TYPE_POINT;
	int created_shadow_map_size = 0;
	RHITextureRef shadow_map = nullptr;
	friend class EditorApplication;
	friend class DefferedLightingRenderer;
	friend class ShadowRenderer;
	friend struct Reflected<LightComponent>;
	struct CascadeData
	{
		glm::mat4 viewProjMatrix;
		glm::mat4 View;
		float splitDepth;
	};
	eastl::array<CascadeData, SHADOW_MAP_CASCADE_COUNT> cascades;
};

REFLECT_BEGIN(LightComponent)
	REFLECT_FIELD(type).items(light_type_items).radio(),
	REFLECT_FIELD(color).color(),
	REFLECT_FIELD(intensity).range(0.01f, 200000.0f).logarithmic(),
	REFLECT_FIELD(attenuation_radius).range(0.001f, 40.0f).format("%.2f m")
		.EDIT_IF(owner.getType() == LIGHT_TYPE_POINT),
	REFLECT_FIELD(shadow_map_size),
REFLECT_END()


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

REFLECT_BEGIN(RigidBodyComponent)
	REFLECT_FIELD(is_static),
	REFLECT_FIELD(mass),
	REFLECT_FIELD(linear_damping).range(0.0f, 1.0f),
	REFLECT_FIELD(angular_damping).range(0.0f, 1.0f),
	REFLECT_FIELD(gravity).label("Use Gravity"),
	REFLECT_FIELD(is_kinematic),
REFLECT_END()

struct BoxColliderComponent
{
	glm::vec3 half_extent = {0.5f, 0.5f, 0.5f};
};

REFLECT_BEGIN(BoxColliderComponent)
	REFLECT_FIELD(half_extent),
REFLECT_END()

#define ALL_COMPONENTS TransformComponent, MeshRendererComponent, LightComponent, RigidBodyComponent, BoxColliderComponent