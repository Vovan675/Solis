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
