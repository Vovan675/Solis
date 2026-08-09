#include "pch.h"
#include "ParametersPanel.h"
#include "Core/Filesystem.h"
#include "UI.h"
#include "Rendering/Model.h"
#include "imgui/ImGuiWrapper.h"
#include "Scene/Components.h"
#include "AssetBrowserPanel.h"

template <typename C, typename F>
static void drawComponent(Entity entity, const char *title, F func)
{
	if (!entity.hasComponent<C>())
		return;

	bool close = true;
	ImGui::PushFont(UI::font_bold);
	bool open = ImGui::CollapsingHeader(title, &close, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding);
	ImGui::PopFont();

	if (open)
		func(entity.getComponent<C>());

	if (!close)
		entity.removeComponent<C>();
}

template <typename C>
static void addComponentButton(Entity entity, const char *title)
{
	bool selected = entity.hasComponent<C>();
	if (ImGui::Selectable(title, false, selected ? ImGuiSelectableFlags_Disabled : 0))
		entity.addComponent<C>();
}

static std::string yamlToString(const YAML::Node &node)
{
	YAML::Emitter emitter;
	emitter << node;
	return emitter.c_str();
}

static bool assetReferenceField(EditorContext &context, const char *label, Engine::GUID current_handle, AssetType accepted_type, std::filesystem::path &picked_path)
{
	bool picked = false;
	std::filesystem::path current_path = AssetManager::getPathFromGUID(current_handle);

	UI::property(label, [&]
	{
		eastl::string field_text = current_path.empty() ? "None" : current_path.filename().string().c_str();

		float open_button_width = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
		if (ImGui::Button(field_text.c_str(), ImVec2(-open_button_width, 0)))
		{
			std::filesystem::path path = Filesystem::openFileDialog().c_str();
			if (!path.empty() && AssetManager::getAssetTypeFromExtension(path.extension().string().c_str()) == accepted_type)
			{
				picked_path = path;
				picked = true;
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH"))
			{
				std::filesystem::path dropped = (const char *)payload->Data;
				if (AssetManager::getAssetTypeFromExtension(dropped.extension().string().c_str()) == accepted_type)
				{
					picked_path = dropped;
					picked = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!current_handle.isValid());
		if (ImGui::Button(ICON_FA_UP_RIGHT_FROM_SQUARE))
		{
			context.selected_path = current_path;
			context.selection_type = EditorSelectionType::Asset;
		}
		ImGui::EndDisabled();
		return picked;
	});

	return picked;
}

static void drawTexturePreview(RHITextureRef texture, int mip, int layer)
{
	float aspect = (float)texture->getWidth() / texture->getHeight();
	float width = ImGui::GetContentRegionAvail().x;
	ImGui::Image(ImGuiWrapper::getTextureId(texture, mip, layer), ImVec2(width, width / aspect));
}

bool ParametersPanel::renderImGui(EditorContext &context, DebugRenderer &debug_renderer, AssetBrowserPanel &asset_browser)
{
	ImGui::Begin((eastl::string(ICON_FA_PEN) + " Parameters###Parameters").c_str());

	auto selected_path = context.selected_path;
	Entity entity = context.selected_entity;
	bool prefer_asset = context.selection_type == EditorSelectionType::Asset && !selected_path.empty();
	if (entity && !prefer_asset)
	{
		drawComponent<TransformComponent>(entity, "Transform", [&](TransformComponent &transform_component) {
			glm::vec3 position = transform_component.getLocalPosition();
			if (UI::inputFloat3("Position", position.data.data))
				transform_component.setPosition(position);

			glm::vec3 rotation = glm::degrees(transform_component.getLocalRotationEuler());
			if (UI::inputFloat3("Rotation", rotation.data.data))
				transform_component.setLocalRotationEuler(glm::radians(rotation));

			glm::vec3 scale = transform_component.getLocalScale();
			if (UI::inputFloat3("Scale", scale.data.data))
				transform_component.setLocalScale(scale);
		});

		drawComponent<MeshRendererComponent>(entity, "Mesh Renderer", [&](MeshRendererComponent &mesh_renderer)
		{
			if (mesh_renderer.meshes.empty())
				ImGui::TextColored(ImVec4(0.90f, 0.35f, 0.30f, 1.0f), "No mesh");

			Engine::GUID model_handle = 0;
			if (!mesh_renderer.meshes.empty())
				model_handle = AssetManager::getGUIDFromPath(mesh_renderer.meshes[0].model->getPath().c_str());

			std::filesystem::path picked_model_path;
			if (assetReferenceField(context, "Mesh", model_handle, ASSET_TYPE_MODEL, picked_model_path))
			{
				auto model = AssetManager::getModelAsset(picked_model_path.string().c_str());
				mesh_renderer.meshes.clear();
				for (auto node : model->getLinearNodes())
				{
					if (!node->primitives.empty())
					{
						mesh_renderer.setFromMeshNode(model, node);
						break;
					}
				}
				entity.markDirty(DIRTY_RENDER_STATE);
			}

			ImGui::SeparatorText("Materials");
			for (int i = 0; i < mesh_renderer.materials.size(); i++)
			{
				auto mat = mesh_renderer.materials[i];
				eastl::string name = "Material " + eastl::to_string(i);

				if (!ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
					continue;

				bool material_changed = false;
				auto show_texture_edit = [&context, &material_changed](Material::MaterialTexture &material_texture, const char *name)
				{
					bool use_texture = material_texture.bindless_id != 0;
					eastl::string label = eastl::string("Use ") + name + " Texture";
					if (UI::checkbox(label.c_str(), &use_texture))
					{
						if (!use_texture)
							material_texture.asset_handle = 0;
						material_texture.bindless_id = use_texture ? 1 : 0;
						material_changed = true;
					}
					if (!use_texture)
						return false;

					std::filesystem::path picked_texture_path;
					if (assetReferenceField(context, name, material_texture.asset_handle, ASSET_TYPE_TEXTURE, picked_texture_path))
					{
						material_texture.asset_handle = AssetManager::getGUIDFromPath(picked_texture_path);
						material_texture.bindless_id = 0;
						material_changed = true;
					}
					return true;
				};

				if (!show_texture_edit(mat->albedo_tex, "Albedo"))
					material_changed |= UI::colorEdit4("Albedo", mat->albedo.data.data);

				show_texture_edit(mat->normal_tex, "Normal");

				if (!show_texture_edit(mat->metalness_tex, "Metalness"))
					material_changed |= UI::sliderFloat("Metalness", &mat->metalness, 0.0f, 1.0f, "%.2f");

				if (!show_texture_edit(mat->roughness_tex, "Roughness"))
					material_changed |= UI::sliderFloat("Roughness", &mat->roughness, 0.0f, 1.0f, "%.2f");

				if (!show_texture_edit(mat->specular_tex, "Specular"))
					material_changed |= UI::sliderFloat("Specular", &mat->specular, 0.0f, 1.0f, "%.2f");

				// TODO: mark only material as dirty, so reupload only material
				if (material_changed)
					entity.markDirty(DIRTY_RENDER_STATE);

				ImGui::TreePop();
			}
		});

		drawComponent<LightComponent>(entity, "Light", [](LightComponent &light) {
			int light_type = light.getType();
			const char *items[] = {"Point", "Directional"};
			if (UI::combo("Type", &light_type, items, IM_ARRAYSIZE(items)))
				light.setType((LIGHT_TYPE)light_type);

			UI::colorEdit3("Color", light.color.data.data);

			const char *intensity_label = light_type == LIGHT_TYPE_DIRECTIONAL ? "Illuminance" : "Luminous Power";
			const char *intensity_format = light_type == LIGHT_TYPE_DIRECTIONAL ? "%.0f lux" : "%.0f lm";
			UI::sliderFloat(intensity_label, &light.intensity, 0.01f, 200000.0f, intensity_format, true);

			if (light_type == LIGHT_TYPE_POINT)
				UI::sliderFloat("Attenuation Radius", &light.attenuation_radius, 0.001f, 40.0f, "%.2f m");

			RHITextureRef texture = light.shadow_map;
			if (!texture)
				return;

			ImGui::SeparatorText("Shadow Map");
			static int layer_index = 0;
			UI::sliderInt("Layer / Face", &layer_index, 0, light_type == LIGHT_TYPE_POINT ? 5 : 3);
			drawTexturePreview(texture, 0, layer_index);
		});

		drawComponent<RigidBodyComponent>(entity, "Rigid Body", [&](RigidBodyComponent &rb) {
			UI::checkbox("Is Static", &rb.is_static);
			UI::checkbox("Is Kinematic", &rb.is_kinematic);
			UI::checkbox("Use Gravity", &rb.gravity);
			UI::dragFloat("Linear Damping", &rb.linear_damping, 0.01f, 0.0f, 1.0f);
			UI::dragFloat("Angular Damping", &rb.angular_damping, 0.05f, 0.0f, 1.0f);
		});

		drawComponent<BoxColliderComponent>(entity, "Box Collider", [&](BoxColliderComponent &collider) {
			UI::inputFloat3("Half Extent", collider.half_extent.data.data);
			if (ImGui::Button("Extent to Bounds", ImVec2(-FLT_MIN, 0)))
			{
				MeshRendererComponent &mesh_renderer = entity.getComponent<MeshRendererComponent>();
				BoundBox bbox;
				for (auto &mesh_id : mesh_renderer.meshes)
				{
					auto mesh = mesh_id.getMesh();
					bbox.extend(mesh->bound_box);
				}

				TransformComponent &transform = entity.getComponent<TransformComponent>();
				collider.half_extent = bbox.getSize() * transform.getLocalScale() / 2.0f;
			}
		});

		ImGui::Spacing();
		if (ImGui::Button(ICON_FA_PLUS " Add Component", ImVec2(-FLT_MIN, 0)))
			ImGui::OpenPopup("add_component_popup");
		if (ImGui::BeginPopup("add_component_popup"))
		{
			addComponentButton<MeshRendererComponent>(entity, "Mesh Renderer");
			addComponentButton<LightComponent>(entity, "Light");
			addComponentButton<RigidBodyComponent>(entity, "Rigid Body");
			addComponentButton<BoxColliderComponent>(entity, "Box Collider");
			ImGui::EndPopup();
		}
	} else if (!selected_path.empty())
	{
		if (AssetManager::getAssetTypeFromExtension(selected_path.extension().string().c_str()) != ASSET_TYPE_UNDEFINED)
			AssetManager::getOrCreateMetadata(selected_path);

		auto &metadata = AssetManager::getMetadata(selected_path);
		if (metadata.isValid())
		{
			ImGui::SeparatorText(selected_path.filename().string().c_str());
			if (ImGui::SmallButton(ICON_FA_MAGNIFYING_GLASS " Show in Asset Browser"))
				asset_browser.setCurrentAsset(selected_path);

			UI::text("Asset Handle", "%llu", metadata.asset_handle);
			UI::text("Runtime Handle", "%llu", metadata.runtime_handle);

			static bool reimported = false;

			static std::filesystem::path last_selected_path;
			static YAML::Node edited_params;
			static std::string base_params_string;
			if (last_selected_path != selected_path)
			{
				last_selected_path = selected_path;
				edited_params = YAML::Clone(metadata.params);
				base_params_string = yamlToString(edited_params);
			}

			if (metadata.type == ASSET_TYPE_MODEL)
			{
				bool generate_meshlets = edited_params["generate_meshlets"].as<bool>(true);
				if (UI::checkbox("Meshlet (Nanite) Geometry", &generate_meshlets))
					edited_params["generate_meshlets"] = generate_meshlets;
			}

			if (metadata.type == ASSET_TYPE_TEXTURE)
			{
				bool generate_mipmaps = edited_params["generate_mipmaps"].as<int>(1);
				if (UI::checkbox("Generate Mipmaps", &generate_mipmaps))
					edited_params["generate_mipmaps"] = (int)generate_mipmaps;

				auto texture = AssetManager::getTextureAsset(selected_path.string().c_str());

				static int mip_index = 0;
				if (reimported)
					mip_index = 0;
				UI::sliderInt("Mip", &mip_index, 0, texture->getDescription().mip_levels - 1);

				if (texture)
					drawTexturePreview(texture, mip_index, -1);
			}

			bool params_dirty = yamlToString(edited_params) != base_params_string;
			reimported = false;
			if (params_dirty)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
			if (ImGui::Button("Reimport", ImVec2(-FLT_MIN, 0)))
			{
				reimported = true;
				metadata.params = YAML::Clone(edited_params);
				AssetManager::saveMetadata(metadata);
				AssetManager::reimport(selected_path);
				base_params_string = yamlToString(edited_params);
			}
			if (params_dirty)
				ImGui::PopStyleColor();
		}
	}

	for (entt::entity selected_id : context.selected_entities)
	{
		Entity selected(selected_id);
		if (selected.hasComponent<MeshRendererComponent>())
		{
			auto &mesh_renderer = selected.getComponent<MeshRendererComponent>();
			for (auto &mesh_id : mesh_renderer.meshes)
			{
				Engine::Mesh *mesh = mesh_id.getMesh();
				if (mesh)
					debug_renderer.addBoundBox(mesh->bound_box * selected.getWorldTransformMatrix());
			}
		}
	}

	ImGui::End();
	return false;
}
