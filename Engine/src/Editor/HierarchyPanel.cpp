#include "pch.h"
#include "HierarchyPanel.h"
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"

void HierarchyPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((std::string(ICON_FA_LIST_UL) + " Hierarchy###Hierarchy").c_str());
	auto view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent>();
	
	std::vector<entt::entity> entities_to_delete;
	std::function<void(TransformComponent &transform)> add_entity_tree = [&context, &add_entity_tree, &entities_to_delete, &view, this] (TransformComponent &transform) {

		ImGui::PushID((int)transform.owner);;
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;

		// Collapsable or not
		flags |= transform.children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;

		// Highlighted or not 
		flags |= context.selected_entity == transform.owner ? ImGuiTreeNodeFlags_Selected : 0;

		std::string name = transform.name;
		if (name.empty())
			name = "(Empty)";

		bool opened = ImGui::TreeNodeEx(&transform.owner, flags, name.c_str());

		bool clicked = ImGui::IsItemClicked();
		if (clicked)
		{
			context.selected_entity = transform.owner;
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Remove"))
			{
				entities_to_delete.push_back(transform.owner);
				if (context.selected_entity == transform.owner)
					context.selected_entity = Entity();
			}
			ImGui::EndPopup();
		}

		if (opened)
		{
			for (entt::entity child_id : transform.children)
			{
				add_entity_tree(view.get<TransformComponent>(child_id));
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	for (auto entity: view)
	{
		TransformComponent &transform = view.get<TransformComponent>(entity);
		if (transform.parent == entt::null)
			add_entity_tree(transform);
	}

	for (auto &entity_id : entities_to_delete)
		Scene::getCurrentScene()->destroyEntity(entity_id);

	if (ImGui::Button("Create Entity"))
	{
		Entity entity = Scene::getCurrentScene()->createEntity("New Entity");
	}

	ImGui::End();

}
