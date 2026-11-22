#pragma once
#include "Scene/Entity.h"
#include "Core/Variables.h"

enum class EditorSelectionType
{
	Entity,
	Asset
};

struct EditorSettings
{
	ViewSettings view_settings;
};

REFLECT_BEGIN(EditorSettings)
	REFLECT_FIELD(view_settings),
REFLECT_END()

struct EditorContext
{
	Camera editor_camera;
	EditorSettings settings;
	EditorSelectionType selection_type = EditorSelectionType::Entity;

	const eastl::vector<entt::entity> &getSelectedEntities() const { return selected_entities; }
	Entity getSelectedEntity() const { return selected_entities.empty() ? Entity() : Entity(selected_entities.back()); }
	bool isSelected(entt::entity id) const { return eastl::find(selected_entities.begin(), selected_entities.end(), id) != selected_entities.end(); }
	void setSelectedEntities(const eastl::vector<entt::entity> &entities)
	{
		if (entities == selected_entities)
			return;

		selected_entities = entities;
		if (!entities.empty())
			selection_type = EditorSelectionType::Entity;
		selection_changed_signal.publish(selected_entities);
	}
	void select(entt::entity id) { setSelectedEntities({id}); }
	void clearSelection() { setSelectedEntities({}); }
	void toggleSelected(entt::entity id)
	{
		eastl::vector<entt::entity> entities = selected_entities;
		auto it = eastl::find(entities.begin(), entities.end(), id);
		if (it != entities.end())
			entities.erase(it);
		else
			entities.push_back(id);
		setSelectedEntities(entities);
	}
	entt::sink<entt::sigh<void (const eastl::vector<entt::entity> &)>> onSelectionChanged() { return {selection_changed_signal}; }

	std::filesystem::path selected_path;

	inline static EditorContext *current = nullptr;

private:
	eastl::vector<entt::entity> selected_entities;
	entt::sigh<void (const eastl::vector<entt::entity> &)> selection_changed_signal;
};
