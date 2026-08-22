#include "pch.h"
#include "Components.h"
#include "Rendering/Model.h"
#include "Assets/AssetManager.h"

Model *MeshRendererComponent::MeshId::getModel()
{
	if (!model)
		model = AssetManager::getAsset<Model>(model_asset).getReference();
	return model;
}

Engine::Mesh *MeshRendererComponent::MeshId::getMesh()
{
	Model *model = getModel();
	return model ? model->getMesh(mesh_id) : nullptr;
}

Material *MeshRendererComponent::MaterialSlot::getMaterial()
{
	if (material && material_asset.guid.isValid() && material->guid != material_asset.guid)
		material = nullptr;

	if (!material && material_asset.isValid())
		material = AssetManager::getAsset<Material>(material_asset);

	return material.getReference();
}

void MeshRendererComponent::MaterialSlot::setMaterial(Ref<Material> mat)
{
	material = mat;
	material_asset = AssetReference(mat.getReference());
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
