#include "pch.h"
#include "Components.h"
#include "Rendering/Model.h"

Engine::Mesh *MeshRendererComponent::MeshId::getMesh()
{
    return model->getMesh(mesh_id);
}

void MeshRendererComponent::setFromMeshNode(Ref<Model> model, MeshNode *mesh_node)
{
	meshes.clear();
	materials.clear();

	for (const MeshNode::Primitive &prim : mesh_node->primitives)
	{
		MeshRendererComponent::MeshId mesh_id;
		mesh_id.model = model;
		mesh_id.mesh_id = prim.mesh->id;
		meshes.push_back(mesh_id);
		materials.push_back(prim.material);
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
