#include "pch.h"
#include "HierarchyPanel.h"
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"

void HierarchyPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((std::string(ICON_FA_LIST_UL) + " Hierarchy###Hierarchy").c_str());
	auto entities_id = Scene::getCurrentScene()->getEntitiesWith<TransformComponent>();

	std::vector<entt::entity> entities_to_delete;
	std::function<void(entt::entity entity_id)> add_entity_tree = [&context, &add_entity_tree, &entities_to_delete, this] (entt::entity entity_id) {

		ImGui::PushID((int)entity_id);;
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;

		Entity entity(entity_id);
		TransformComponent &transform = entity.getComponent<TransformComponent>();

		// Collapsable or not
		flags |= transform.children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;

		// Highlighted or not 
		flags |= context.selected_entity == entity ? ImGuiTreeNodeFlags_Selected : 0;

		std::string name = transform.name;
		if (name.empty())
			name = "(Empty)";

		bool opened = ImGui::TreeNodeEx(&entity, flags, name.c_str());

		bool clicked = ImGui::IsItemClicked();
		if (clicked)
		{
			context.selected_entity = entity;
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Remove"))
			{
				entities_to_delete.push_back(entity);
				if (context.selected_entity == entity)
					context.selected_entity = Entity();
			}
			ImGui::EndPopup();
		}

		if (opened)
		{
			for (entt::entity child_id : transform.children)
			{
				add_entity_tree(child_id);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	for (const auto &entity_id : entities_id)
	{
		Entity entity(entity_id);
		TransformComponent &transform = entity.getComponent<TransformComponent>();
		if (transform.parent == entt::null)
			add_entity_tree(entity_id);
	}

	for (auto &entity_id : entities_to_delete)
		Scene::getCurrentScene()->destroyEntity(entity_id);

	if (ImGui::Button("Create Entity"))
	{
		Entity entity = Scene::getCurrentScene()->createEntity("New Entity");
	}

	ImGui::End();

}
