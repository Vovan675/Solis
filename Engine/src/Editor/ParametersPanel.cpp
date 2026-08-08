#include "pch.h"
#include "ParametersPanel.h"
#include "Core/Filesystem.h"
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"
#include "Rendering/Model.h"
#include "imgui/ImGuiWrapper.h"
#include "Scene/Components.h"
#include "AssetBrowserPanel.h"

template <typename C, typename F>
static void drawComponent(Entity entity, const char *title, F func)
{
	if (entity.hasComponent<C>())
	{
		bool close = true;
		if (ImGui::CollapsingHeader(title, &close, ImGuiTreeNodeFlags_DefaultOpen))
		{
			func(entity.getComponent<C>());
		}
		if (!close)
		{
			entity.removeComponent<C>();
		}
	}
}

static void alignForWidth(float width, float alignment = 0.5f)
{
	float avail = ImGui::GetContentRegionAvail().x;
	float off = (avail - width) * alignment;
	if (off > 0.0f)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
}

static bool centeredButton(const char *label, float alignment = 0.5f)
{
	ImGuiStyle& style = ImGui::GetStyle();
	float width = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	alignForWidth(width, alignment);
	return ImGui::Button(label);
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
	ImGui::PushID(label);
	bool picked = false;

	std::filesystem::path current_path = AssetManager::getPathFromGUID(current_handle);
	eastl::string field_text = current_path.empty() ? "None" : current_path.filename().string().c_str();

	ImGui::Text("%s", label);
	ImGui::SameLine();
	if (ImGui::Button(field_text.c_str()))
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

	ImGui::PopID();
	return picked;
}

bool ParametersPanel::renderImGui(EditorContext &context, DebugRenderer &debug_renderer, AssetBrowserPanel &asset_browser)
{
	ImGui::Begin((eastl::string(ICON_FA_PEN) + " Parameters###Parameters").c_str());
	bool is_using_ui = ImGui::IsWindowFocused();

	auto selected_path = context.selected_path;
	Entity entity = context.selected_entity;
	bool prefer_asset = context.selection_type == EditorSelectionType::Asset && !selected_path.empty();
	if (entity && !prefer_asset)
	{
		drawComponent<TransformComponent>(entity, "Transform", [&](TransformComponent &transform_component) {
			glm::vec3 position = transform_component.getLocalPosition();
			if (ImGui::InputFloat3("Position", position.data.data))
				transform_component.setPosition(position);

			glm::vec3 rot = glm::degrees(transform_component.getLocalRotationEuler());
			if(ImGui::InputFloat3("Rotation", rot.data.data))
				transform_component.setLocalRotationEuler(glm::radians(rot));
			
			glm::vec3 scale = transform_component.getLocalScale();
			if (ImGui::InputFloat3("Scale", scale.data.data))
				transform_component.setLocalScale(scale);
		});

		drawComponent<MeshRendererComponent>(entity, "Mesh Renderer", [&](MeshRendererComponent &mesh_renderer)
		{
			ImGui::SeparatorText("Mesh Settings");
			if (mesh_renderer.meshes.empty())
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "No mesh");

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

				if (ImGui::TreeNode(name.c_str()))
				{
					bool material_changed = false;
					auto show_texture_edit = [&context, &material_changed](Material::MaterialTexture &material_texture, const char *name)
					{
						bool use_texture = material_texture.bindless_id != 0;
						eastl::string label = eastl::string("Use ") + name + " texture";
						if (ImGui::Checkbox(label.c_str(), &use_texture))
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

					if (!show_texture_edit(mat->albedo_tex, "albedo"))
						material_changed |= ImGui::ColorEdit4("Color", mat->albedo.data.data);

					show_texture_edit(mat->normal_tex, "normal");

					if (!show_texture_edit(mat->metalness_tex, "metalness"))
						material_changed |= ImGui::DragFloat("Metalness", &mat->metalness, 0.1, 0, 1.0);

					if (!show_texture_edit(mat->roughness_tex, "roughness"))
						material_changed |= ImGui::DragFloat("Roughness", &mat->roughness, 0.1, 0, 1.0);

					if (!show_texture_edit(mat->specular_tex, "specular"))
						material_changed |= ImGui::DragFloat("Specular", &mat->specular, 0.1, 0, 1.0);

					// TODO: mark only material as dirty, so reupload only material
					if (material_changed)
						entity.markDirty(DIRTY_RENDER_STATE);

					ImGui::TreePop();
				}
			}
		});

		drawComponent<LightComponent>(entity, "Light", [](LightComponent &light) {
			int light_type = light.getType();
			char *items[] = {"Point", "Directional"};
			if (ImGui::BeginCombo("Light type", items[light_type]))
			{
				for (int n = 0; n < IM_ARRAYSIZE(items); n++)
				{
					bool is_selected = (light_type == n);
					if (ImGui::Selectable(items[n], is_selected))
						light_type = n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			light.setType((LIGHT_TYPE)light_type);
			ImGui::ColorEdit3("Light Color", light.color.data.data);
			ImGui::SliderFloat("Attenuation Radius", &light.attenuation_radius, 0.001f, 40.0);
			const char *intensity_label = light_type == LIGHT_TYPE_DIRECTIONAL ? "Light Intensity (lux)" : "Light Intensity (lumens)";
			ImGui::SliderFloat(intensity_label, &light.intensity, 0.01f, 200000.0f, "%.2f", ImGuiSliderFlags_Logarithmic);

			RHITextureRef texture = light.shadow_map;
			if (texture)
			{
				static int layer_index = 0;
				ImGui::SliderInt("Layer/Face", &layer_index, 0, light_type == LIGHT_TYPE_POINT ? 5 : 3);

				float aspect = (float)texture->getWidth() / (float)texture->getHeight();
				ImVec2 viewport_size = ImGui::GetContentRegionAvail();
				float min_size = std::min(viewport_size.x, viewport_size.y);
				viewport_size.x = min_size;
				viewport_size.y = min_size / aspect;

				ImGui::Image(ImGuiWrapper::getTextureId(texture, 0, layer_index), viewport_size, {0, 0}, {1, 1});
			}
		});


		drawComponent<RigidBodyComponent>(entity, "Rigid Body", [&](RigidBodyComponent &rb) {
			ImGui::Checkbox("Is Static", &rb.is_static);
			ImGui::DragFloat("Linear Damping", &rb.linear_damping, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Angular Damping", &rb.angular_damping, 0.05f, 0.0f, 1.0f);
			ImGui::Checkbox("Use Gravity", &rb.gravity);
			ImGui::Checkbox("Is Kinematic", &rb.is_kinematic);
		});

		drawComponent<BoxColliderComponent>(entity, "Box Collider", [&](BoxColliderComponent &collider) {
			ImGui::InputFloat3("Half Extent", collider.half_extent.data.data);
			if (ImGui::Button("Extent to Bounds"))
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


		if (centeredButton("Add component..."))
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

			ImGui::Text("Asset Handle: %llu", metadata.asset_handle);
			ImGui::Text("Runtime Handle: %llu", metadata.runtime_handle);

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
				if (ImGui::Checkbox("Meshlet (Nanite) geometry", &generate_meshlets))
					edited_params["generate_meshlets"] = generate_meshlets;
			}

			if (metadata.type == ASSET_TYPE_TEXTURE)
			{
				bool generate_mipmaps = edited_params["generate_mipmaps"].as<int>(1);
				if (ImGui::Checkbox("Generate Mipmaps", &generate_mipmaps))
					edited_params["generate_mipmaps"] = (int)generate_mipmaps;

				auto texture = AssetManager::getTextureAsset(selected_path.string().c_str());

				static int mip_index = 0;
				if (reimported)
					mip_index = 0;
				ImGui::SliderInt("Mip", &mip_index, 0, texture->getDescription().mip_levels - 1);

				float aspect = (float)texture->getWidth() / (float)texture->getHeight();
				ImVec2 viewport_size = ImGui::GetContentRegionAvail();
				float min_size = std::min(viewport_size.x, viewport_size.y);
				viewport_size.x = min_size;
				viewport_size.y = min_size / aspect;

				if (texture)
					ImGui::Image(ImGuiWrapper::getTextureId(texture, mip_index), viewport_size, {0, 0}, {1, 1});
			}

			bool params_dirty = yamlToString(edited_params) != base_params_string;
			reimported = false;
			if (params_dirty)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
			if (ImGui::Button("Reimport"))
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
