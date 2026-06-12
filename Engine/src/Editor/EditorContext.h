#pragma once
#include "Scene/Entity.h"

enum class EditorSelectionType
{
	Entity,
	Asset
};

struct EditorContext
{
	Camera editor_camera;
	EditorSelectionType selection_type = EditorSelectionType::Entity;

	Entity selected_entity;
	eastl::vector<entt::entity> selected_entities;

	std::filesystem::path selected_path;
};
