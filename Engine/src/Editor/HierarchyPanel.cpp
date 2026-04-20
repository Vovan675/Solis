#include "pch.h"
#include "HierarchyPanel.h"
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"

void HierarchyPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((eastl::string(ICON_FA_LIST_UL) + " Hierarchy###Hierarchy").c_str());
	auto view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent>();

	auto is_selected = [&](entt::entity entity)
	{
		for (entt::entity id : context.selected_entities)
		{
			if (id == entity)
				return true;
		}
		return false;
	};

	auto select_range = [&](entt::entity clicked)
	{
		// Flat list for shift selection
		eastl::vector<entt::entity> flat_list;
		eastl::function<void(entt::entity)> build_flat = [&](entt::entity id)
		{
			flat_list.push_back(id);
			for (entt::entity child_id : view.get<TransformComponent>(id).children)
				build_flat(child_id);
		};

		for (entt::entity entity : view)
		{
			if (view.get<TransformComponent>(entity).parent == entt::null)
				build_flat(entity);
		}


		entt::entity begin = (start_entity != entt::null) ? start_entity : clicked;
		int begin_id = -1, end_id = -1;
		for (int i = 0; i < flat_list.size(); i++)
		{
			if (flat_list[i] == begin)
				begin_id = i;
			if (flat_list[i] == clicked)
				end_id = i;
		}
		if (begin_id == -1 || end_id == -1)
			return;
		if (begin_id > end_id)
			std::swap(begin_id, end_id);

		context.selected_entities.clear();
		for (int i = begin_id; i <= end_id; i++)
			context.selected_entities.push_back(flat_list[i]);
	};

	auto toggle_selected = [&](entt::entity entity)
	{
		for (int i = 0; i < context.selected_entities.size(); i++)
		{
			if (context.selected_entities[i] == entity)
			{
				context.selected_entities.erase(context.selected_entities.begin() + i);
				return;
			}
		}
		context.selected_entities.push_back(entity);
	};

	eastl::vector<entt::entity> entities_to_delete;
	eastl::function<void(TransformComponent &transform)> add_entity_tree = [&](TransformComponent &transform)
	{

		ImGui::PushID((int)transform.owner);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;

		// Collapsable or not
		flags |= transform.children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;
		// Highlighted or not
		flags |= is_selected(transform.owner) ? ImGuiTreeNodeFlags_Selected : 0;

		eastl::string name = transform.name;
		if (name.empty())
			name = "(Empty)";

		bool opened = ImGui::TreeNodeEx(&transform.owner, flags, name.c_str());

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			ImGuiIO &io = ImGui::GetIO();
			if (io.KeyShift)
			{
				select_range(transform.owner);
			} else if (io.KeyCtrl)
			{
				toggle_selected(transform.owner);
				start_entity = transform.owner;
			} else
			{
				context.selected_entities.clear();
				context.selected_entities.push_back(transform.owner);
				start_entity = transform.owner;
			}
			context.selected_entity = Entity(transform.owner);
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Remove"))
			{
				if (is_selected(transform.owner))
				{
					for (entt::entity id : context.selected_entities)
						entities_to_delete.push_back(id);
				} else
				{
					entities_to_delete.push_back(transform.owner);
				}
			}
			ImGui::EndPopup();
		}

		if (opened)
		{
			for (entt::entity child_id : transform.children)
				add_entity_tree(view.get<TransformComponent>(child_id));
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	for (entt::entity entity : view)
	{
		TransformComponent &transform = view.get<TransformComponent>(entity);
		if (transform.parent == entt::null)
			add_entity_tree(transform);
	}

	if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		for (entt::entity id : context.selected_entities)
			entities_to_delete.push_back(id);
	}

	for (entt::entity entity_id : entities_to_delete)
		Scene::getCurrentScene()->destroyEntity(entity_id);

	if (!entities_to_delete.empty())
	{
		context.selected_entities.clear();
		context.selected_entity = Entity();
		start_entity = entt::null;
	}

	if (ImGui::Button("Create Entity"))
		Scene::getCurrentScene()->createEntity("New Entity");

	ImGui::End();
}
