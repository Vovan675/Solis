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

	eastl::vector<entt::entity> selected_entities;
	Entity getSelectedEntity() const { return selected_entities.empty() ? Entity() : Entity(selected_entities.back()); }
	bool isSelected(entt::entity id) const { return eastl::find(selected_entities.begin(), selected_entities.end(), id) != selected_entities.end(); }
	void toggleSelected(entt::entity id)
	{
		auto it = eastl::find(selected_entities.begin(), selected_entities.end(), id);
		if (it != selected_entities.end())
			selected_entities.erase(it);
		else
			selected_entities.push_back(id);
	}

	std::filesystem::path selected_path;

	inline static EditorContext *current = nullptr;
};
