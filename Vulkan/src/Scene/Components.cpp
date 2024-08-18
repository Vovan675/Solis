#include "pch.h"
#include "Components.h"
#include "Model.h"

std::shared_ptr<Engine::Mesh> MeshRendererComponent::MeshId::getMesh()
{
    return model->getMesh(mesh_id);
}
