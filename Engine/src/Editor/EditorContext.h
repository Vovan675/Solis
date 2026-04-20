#pragma once
#include "Scene/Entity.h"

struct EditorContext
{
	Entity selected_entity;
	eastl::vector<entt::entity> selected_entities;
	std::filesystem::path selected_path;
	Camera editor_camera;
};
