#include "pch.h"
#include "Components.h"
#include "Model.h"

Engine::Mesh *MeshRendererComponent::MeshId::getMesh()
{
    return model->getMesh(mesh_id);
}

void MeshRendererComponent::setFromMeshNode(Ref<Model> model, MeshNode *mesh_node)
{
	meshes.clear();
	materials.clear();

	for (int i = 0; i < mesh_node->meshes.size(); i++)
	{
		MeshRendererComponent::MeshId mesh_id;
		mesh_id.model = model;
		mesh_id.mesh_id = mesh_node->meshes[i]->id;
		meshes.push_back(mesh_id);
	}
	materials = mesh_node->materials;
}
