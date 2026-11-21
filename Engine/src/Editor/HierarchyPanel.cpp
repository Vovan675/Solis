#include "pch.h"
#include "HierarchyPanel.h"
#include "UI.h"

void HierarchyPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((eastl::string(ICON_FA_LIST_UL) + " Hierarchy###Hierarchy").c_str());
	ImGui::PushStyleColor(ImGuiCol_Header, UI::selection_color);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::selection_color);
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputTextWithHint("##Search", ICON_FA_MAGNIFYING_GLASS " Search", filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf)))
		filter.Build();
	ImGui::Separator();
	auto view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent>();

	auto sort_by_name = [&](eastl::vector<entt::entity> entities)
	{
		eastl::sort(entities.begin(), entities.end(), [&](entt::entity a, entt::entity b)
		{
			int order = prettyCompare(view.get<TransformComponent>(a).name.c_str(), view.get<TransformComponent>(b).name.c_str());
			return order != 0 ? order < 0 : a < b;
		});
		return entities;
	};

	eastl::vector<entt::entity> roots;
	for (entt::entity entity : view)
	{
		TransformComponent &transform = view.get<TransformComponent>(entity);
		if (filter.IsActive() ? filter.PassFilter(transform.name.c_str()) : transform.parent == entt::null)
			roots.push_back(entity);
	}
	roots = sort_by_name(roots);

	auto select_range = [&](entt::entity clicked)
	{
		// Flat list for shift selection
		eastl::vector<entt::entity> flat_list;
		eastl::function<void(entt::entity)> build_flat = [&](entt::entity id)
		{
			flat_list.push_back(id);
			if (filter.IsActive())
				return;

			eastl::vector<entt::entity> sorted_children = sort_by_name(view.get<TransformComponent>(id).children);
			for (entt::entity child_id : sorted_children)
				build_flat(child_id);
		};

		for (entt::entity root : roots)
			build_flat(root);

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
		{
			if (flat_list[i] != clicked)
			context.selected_entities.push_back(flat_list[i]);
		}
		context.selected_entities.push_back(clicked);
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

	auto draw_create_menu = [&](entt::entity parent_id)
	{
		if (!ImGui::BeginMenu("Create"))
			return;

		Ref<Scene> scene = Scene::getCurrentScene();
		Entity entity;
		if (ImGui::MenuItem("Empty"))
			entity = scene->createEntity("New Entity");
		if (ImGui::MenuItem("Point Light"))
		{
			entity = scene->createEntity("Point Light");
			LightComponent &light = entity.addComponent<LightComponent>();
			light.intensity = 1000.0f;
			light.attenuation_radius = 10.0f;
		}
		if (ImGui::MenuItem("Directional Light"))
		{
			entity = scene->createEntity("Directional Light");
			entity.getTransform().setLocalRotationEuler(glm::radians(glm::vec3(-111.0f, 0.0f, 175.0f)));
			LightComponent &light = entity.addComponent<LightComponent>();
			light.setType(LIGHT_TYPE_DIRECTIONAL);
			light.intensity = 100000.0f;
		}

		if (entity)
		{
			entity.getTransform().setParent(parent_id);
			context.selected_entities = {entity};
			context.selection_type = EditorSelectionType::Entity;
			start_entity = entity;
		}

		ImGui::EndMenu();
	};

	auto accept_entity_drop = [&](entt::entity parent_id)
	{
		if (!ImGui::BeginDragDropTarget())
			return;

		const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("DND_ENTITY", ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
		entt::entity dragged_entity = payload ? *(const entt::entity *)payload->Data : entt::null;
		entt::entity parent = parent_id;
		while (parent != entt::null && parent != dragged_entity)
			parent = view.get<TransformComponent>(parent).parent;
		bool valid = payload && parent != dragged_entity && view.get<TransformComponent>(dragged_entity).parent != parent_id; // Don't allow to drop into my parent or my own
		if (valid)
		{
			if (parent_id != entt::null)
				ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f)));
			if (payload->IsDelivery())
			{
				TransformComponent &transform = view.get<TransformComponent>(dragged_entity);
				glm::mat4 world_transform = transform.getWorldTransform();
				transform.setParent(parent_id);
				transform.setWorldTransform(world_transform);
				if (parent_id != entt::null)
					ImGui::GetStateStorage()->SetBool(ImGui::GetItemID(), true); // Open parent tree after drop
			}
		}
		ImGui::EndDragDropTarget();
	};

	eastl::vector<entt::entity> entities_to_delete;
	eastl::function<void(TransformComponent &transform)> add_entity_tree = [&](TransformComponent &transform)
	{
		ImGui::PushID((int)transform.owner);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;

		// Collapsable or not
		flags |= transform.children.empty() || filter.IsActive() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;
		// Highlighted or not
		bool selected = context.isSelected(transform.owner);
		flags |= selected ? ImGuiTreeNodeFlags_Selected : 0;

		eastl::string name = transform.name;
		if (name.empty())
			name = "(Empty)";

		float label_x = ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing();
		ImVec2 row_min = ImGui::GetCursorScreenPos();
		ImVec2 label_min(row_min.x + ImGui::GetTreeNodeToLabelSpacing(), row_min.y);
		ImVec2 eye_size(ImGui::GetFrameHeight(), ImGui::GetTextLineHeight());
		ImVec2 eye_min(row_min.x + ImGui::GetContentRegionAvail().x - eye_size.x, row_min.y);
		bool eye_hovered = ImGui::GetMousePos().x >= eye_min.x;

		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selected ? UI::selection_color : ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
		bool opened = ImGui::TreeNodeEx("Entity", flags, "%s", "");
		ImGui::PopStyleColor();
		bool row_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("DND_ENTITY", &transform.owner, sizeof(entt::entity));
			ImGui::TextUnformatted(name.c_str());
			ImGui::EndDragDropSource();
		}
		accept_entity_drop(transform.owner);

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && !eye_hovered)
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
			context.selection_type = EditorSelectionType::Entity;
		}

		if (ImGui::BeginPopupContextItem())
		{
			draw_create_menu(transform.owner);
			if (ImGui::MenuItem("Rename", "F2"))
				renaming_entity = transform.owner;
			if (ImGui::MenuItem("Remove"))
			{
				if (context.isSelected(transform.owner))
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

		if (renaming_entity != transform.owner)
		{
			ImDrawList *draw_list = ImGui::GetWindowDrawList();
			ImVec4 label_clip(label_min.x, label_min.y, eye_min.x, label_min.y + eye_size.y);
			ImU32 label_color = ImGui::GetColorU32(transform.isEnabled() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
			draw_list->AddText(nullptr, 0.0f, label_min, label_color, name.c_str(), nullptr, 0.0f, &label_clip);

			ImGui::SetCursorScreenPos(eye_min);
			if (ImGui::InvisibleButton("##enabled", eye_size))
				transform.setEnabled(!transform.isEnabledSelf());

			if (row_hovered || !transform.isEnabledSelf())
			{
				const char *icon = transform.isEnabledSelf() ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
				ImVec2 icon_min(eye_min.x + (eye_size.x - ImGui::CalcTextSize(icon).x) * 0.5f, eye_min.y);
				draw_list->AddText(icon_min, ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled), icon);
			}
		} else
		{
			float padding_x = ImGui::GetStyle().FramePadding.x;
			ImGui::SameLine(label_x - padding_x);
			ImGui::SetNextItemWidth(-ImGui::GetStyle().ItemSpacing.x);
			if (!ImGui::IsAnyItemActive())
			{
				snprintf(rename_buffer, sizeof(rename_buffer), "%s", transform.name.c_str());
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padding_x, 0.0f));
			ImGui::InputText("##Rename", rename_buffer, sizeof(rename_buffer), ImGuiInputTextFlags_AutoSelectAll);
			ImGui::PopStyleVar();
			if (ImGui::IsItemDeactivatedAfterEdit())
				transform.name = rename_buffer;
			if (ImGui::IsItemDeactivated())
				renaming_entity = entt::null;
		}

		if (opened)
		{
			if (!filter.IsActive())
			{
				eastl::vector<entt::entity> sorted_children = sort_by_name(transform.children);
				for (entt::entity child_id : sorted_children)
					add_entity_tree(view.get<TransformComponent>(child_id));
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	for (entt::entity root : roots)
		add_entity_tree(view.get<TransformComponent>(root));

	// Empty drop zone
	ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, eastl::max(ImGui::GetContentRegionAvail().y, ImGui::GetFrameHeight())));
	accept_entity_drop(entt::null);

	if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		draw_create_menu(entt::null);
		ImGui::EndPopup();
	}

	if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_F2) && context.getSelectedEntity())
			renaming_entity = context.getSelectedEntity();

		if (ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			for (entt::entity id : context.selected_entities)
				entities_to_delete.push_back(id);
		}
	}

	for (entt::entity entity_id : entities_to_delete)
		Scene::getCurrentScene()->destroyEntity(entity_id);

	if (!entities_to_delete.empty())
	{
		context.selected_entities.clear();
		start_entity = entt::null;
	}

	ImGui::PopStyleColor(2);
	ImGui::End();
}
