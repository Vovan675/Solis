#include "pch.h"
#include "Components.h"
#include "Rendering/Model.h"
#include "Assets/AssetManager.h"
#include "Scene/Entity.h"

void TransformComponent::setParent(entt::entity new_parent)
{
	if (parent != entt::null)
		Entity(parent).removeChild(owner);

	parent = new_parent;
	if (parent != entt::null)
		Entity(parent).getTransform().children.push_back(owner);
	Scene::propagate_local_transforms_update(owner);
}

Model *MeshRendererComponent::MeshId::getModel()
{
	return model_asset.load<Model>();
}

Engine::Mesh *MeshRendererComponent::MeshId::getMesh()
{
	Model *model = getModel();
	return model ? model->getMesh(mesh_id) : nullptr;
}

Material *MeshRendererComponent::MaterialSlot::getMaterial()
{
	return material_asset.isValid() ? material_asset.load<Material>() : runtime_material.getReference();
}

void MeshRendererComponent::MaterialSlot::setMaterial(Ref<Material> mat)
{
	material_asset = AssetReference(mat.getReference());
	runtime_material = material_asset.isValid() ? nullptr : mat;
}

void MeshRendererComponent::setMaterial(int index, Ref<Material> material)
{
	if (index >= materials.size())
		materials.resize(index + 1);
	materials[index].setMaterial(material);
}

Material *MeshRendererComponent::getMaterial(int index)
{
	if (index < materials.size())
	{
		if (Material *material = materials[index].getMaterial())
			return material;
	}

	Model *model = meshes[index].getModel();
	return model ? model->getMaterial(meshes[index].mesh_id) : nullptr;
}

void MeshRendererComponent::setFromMeshNode(Ref<Model> model, MeshNode *mesh_node)
{
	meshes.clear();
	materials.clear();

	for (const MeshNode::Primitive &prim : mesh_node->primitives)
	{
		MeshRendererComponent::MeshId mesh_id;
		mesh_id.model_asset = AssetReference(model.getReference());
		mesh_id.mesh_id = prim.mesh->id;
		meshes.push_back(mesh_id);
	}
}

glm::vec3 LightComponent::getPhotometricIntensity() const
{
	float luminance = glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
	if (luminance <= 0.0f)
		return glm::vec3(0.0f);

	glm::vec3 normalized_color = color / luminance;

	if (type == LIGHT_TYPE_POINT)
		return normalized_color * intensity / (4.0f * glm::pi<float>());

	return normalized_color * intensity;
}
