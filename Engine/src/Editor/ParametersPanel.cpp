#include "pch.h"
#include "ParametersPanel.h"
#include "UI.h"
#include "Rendering/Model.h"
#include "Assets/MeshSerializer.h"
#include "imgui/ImGuiWrapper.h"
#include "Scene/Components.h"
#include "AssetBrowserPanel.h"

static eastl::string componentTitle(const char *type_name)
{
	eastl::string title = type_name;
	size_t suffix = title.rfind("Component");
	if (suffix != eastl::string::npos)
		title.erase(suffix);
	return prettifyName(title.c_str());
}

template <typename C, typename F>
static void drawComponent(Entity entity, F func)
{
	if (!entity.hasComponent<C>())
		return;

	eastl::string title = componentTitle(Reflected<C>::getInfo().name);
	bool close = true;
	ImGui::PushFont(UI::font_bold);
	bool open = ImGui::CollapsingHeader(title.c_str(), std::is_same_v<C, TransformComponent> ? nullptr : &close, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding);
	ImGui::PopFont();

	if (open)
		func(entity.getComponent<C>());

	if (!close)
		entity.removeComponent<C>();
}

template<typename... Component>
static void addComponentItems(Entity entity)
{
	([&]()
	{
		bool present = entity.hasComponent<Component>();
		eastl::string title = componentTitle(Reflected<Component>::getInfo().name);
		if (ImGui::Selectable(title.c_str(), false, present ? ImGuiSelectableFlags_Disabled : 0))
			entity.addComponent<Component>();
	}(), ...);
}

static void draw_serialized_asset(const AssetTypeInfo &type, const std::filesystem::path &path)
{
	ImGui::SeparatorText(path.filename().string().c_str());
	UI::text("Type", "%s", type.name);
	UI::text("Asset Handle", "%llu", AssetManager::getOrCreateMetadata(path).guid);

	UI::drawAsset(type, path);
}

static bool draw_light_intensity(const FieldInfo &field, const char *label, void *value, void *owner)
{
	bool is_directional = ((LightComponent *)owner)->getType() == LIGHT_TYPE_DIRECTIONAL;
	return UI::sliderFloat(is_directional ? "Illuminance" : "Luminous Power", (float *)value, field.minValue, field.maxValue, is_directional ? "%.0f lux" : "%.0f lm", field.isLogarithmic, field.tooltipText);
}

static const bool light_intensity_drawer_registered = UI::registerFieldDrawer<LightComponent>("intensity", draw_light_intensity);

static bool draw_material_texture(const FieldInfo &field, const char *label, void *value, void *owner)
{
	MaterialTexture *texture = (MaterialTexture *)value;
	return UI::assetField(label, &texture->asset, AssetManager::getTypeInfo<RHITexture>(), field.tooltipText);
}

static const bool material_texture_drawer_registered = UI::registerTypeDrawer<MaterialTexture>(draw_material_texture);

static void draw_texture_preview(RHITextureRef texture, int mip, int layer)
{
	float aspect = (float)texture->getWidth() / texture->getHeight();
	float width = ImGui::GetContentRegionAvail().x;
	ImGui::Image(ImGuiWrapper::getTextureId(texture, mip, layer), ImVec2(width, width / aspect));
}

static bool draw_transform_vector(const FieldInfo &field, const char *label, void *value, void *owner)
{
	return UI::dragFloat3(label, (float *)value, 0.01f);
}

static bool draw_transform_rotation(const FieldInfo &field, const char *label, void *value, void *owner)
{
	glm::vec3 &rotation = *(glm::vec3 *)value;
	glm::vec3 degrees = glm::degrees(rotation);
	if (!UI::dragFloat3(label, &degrees.x, 0.5f))
		return false;

	rotation = glm::radians(degrees);
	return true;
}

static const bool transform_position_drawer_registered = UI::registerFieldDrawer<TransformComponent>("local_position", draw_transform_vector);
static const bool transform_rotation_drawer_registered = UI::registerFieldDrawer<TransformComponent>("local_rotation_euler", draw_transform_rotation);
static const bool transform_scale_drawer_registered = UI::registerFieldDrawer<TransformComponent>("local_scale", draw_transform_vector);

static void draw_field_key_button(Entity entity, int sequence_frame, const StructInfo &info, const FieldInfo &field, void *owner)
{
	if (!PropertyTrack::isAnimatable(field))
		return;

	Sequence &sequence = Scene::getCurrentScene()->sequence;
	SequenceBinding *binding = sequence.findBinding(entity);
	PropertyTrack *track = binding ? binding->findTrack(info.name, field.name) : nullptr;
	bool is_keyed = track && track->hasKey(sequence_frame);

	bool clicked = UI::keyButton(is_keyed ? UI::KeyState::KEYED : (track ? UI::KeyState::ANIMATED : UI::KeyState::NONE));
	if (clicked && is_keyed)
	{
		track->removeKeys(sequence_frame);
		sequence.removeEmptyTracks();
	} else if (clicked)
	{
		SequenceBinding &entity_binding = sequence.findOrAddBinding(entity);
		entity_binding.findOrAddTrack(info.name, field.name).setKey(field, owner, sequence_frame);
		SequencePlayer::sortTracks(entity_binding);
	}
}

bool ParametersPanel::renderImGui(EditorContext &context, DebugRenderer &debug_renderer, AssetBrowserPanel &asset_browser)
{
	ImGui::Begin((eastl::string(ICON_FA_PEN) + " Parameters###Parameters").c_str());

	auto selected_path = context.selected_path;
	const AssetTypeInfo *asset_type = AssetManager::findTypeInfoByExtension(selected_path.extension().string().c_str());
	Entity entity = context.getSelectedEntity();
	bool prefer_asset = context.selection_type == EditorSelectionType::Asset && !selected_path.empty();
	if (entity && !prefer_asset)
	{
		int sequence_frame = (int)roundf(Scene::getCurrentScene()->sequence_player.frame);

		UI::FieldButtons field_buttons;
		field_buttons.count = 1;
		field_buttons.draw = [=](const StructInfo &info, const FieldInfo &field, void *owner) { draw_field_key_button(entity, sequence_frame, info, field, owner); };

		drawComponent<TransformComponent>(entity, [&](TransformComponent &transform_component) {
			UI::drawStruct(transform_component, field_buttons);
		});

		drawComponent<MeshRendererComponent>(entity, [&](MeshRendererComponent &mesh_renderer)
		{
			if (mesh_renderer.meshes.empty())
				ImGui::TextColored(UI::error_color, "No mesh");

			AssetReference model_reference;
			if (!mesh_renderer.meshes.empty())
				model_reference = mesh_renderer.meshes[0].model_asset;

			if (UI::assetField("Mesh", &model_reference, AssetManager::getTypeInfo<Model>()))
			{
				mesh_renderer.meshes.clear();
				Ref<Model> model = model_reference.load<Model>();
				if (model)
				{
					for (auto node : model->getLinearNodes())
					{
						if (!node->primitives.empty())
						{
							mesh_renderer.setFromMeshNode(model, node);
							break;
						}
					}
				}
				entity.markDirty(DIRTY_RENDER_STATE);
			}

			if (UI::checkbox("Cast Shadows", &mesh_renderer.cast_shadows))
				entity.markDirty(DIRTY_MATERIAL);

			ImGui::SeparatorText("Materials");
			for (int i = 0; i < mesh_renderer.meshes.size(); i++)
			{
				ImGui::PushID(i);
				eastl::string slot_name = "Slot " + eastl::to_string(i);

				AssetReference slot_asset;
				if (i < mesh_renderer.materials.size())
					slot_asset = mesh_renderer.materials[i].material_asset;

				auto duplicate_menu = [&]
				{
					Material *material = mesh_renderer.getMaterial(i);
					if (!ImGui::MenuItem(ICON_FA_COPY " Duplicate Material", nullptr, false, material != nullptr))
						return;

					AssetReference copy(asset_browser.createAsset(*AssetManager::getTypeInfo<Material>(), material));
					mesh_renderer.setMaterial(i, copy.load<Material>());
					entity.markDirty(DIRTY_MATERIAL);
				};

				if (UI::assetField(slot_name.c_str(), &slot_asset, AssetManager::getTypeInfo<Material>(), nullptr, duplicate_menu))
				{
					mesh_renderer.setMaterial(i, slot_asset.load<Material>());
					entity.markDirty(DIRTY_MATERIAL);
				}

				// Draw material from object, its runtime only, so non editable
				if (!slot_asset.isValid())
				{
					ImGui::Indent();
					ImGui::BeginDisabled(true);
					if (Material *material = mesh_renderer.getMaterial(i))
						UI::drawStruct(Reflected<Material>::getInfo(), material);
					ImGui::EndDisabled();
					ImGui::Unindent();
				}
				ImGui::PopID();
			}
		});

		drawComponent<LightComponent>(entity, [&](LightComponent &light) {
			UI::drawStruct(light, field_buttons);

			RHITextureRef texture = light.getShadowMap();
			if (!texture)
				return;

			ImGui::SeparatorText("Shadow Map");
			static int layer_index = 0;
			UI::sliderInt("Layer / Face", &layer_index, 0, light.getType() == LIGHT_TYPE_POINT ? 5 : 3);
			draw_texture_preview(texture, 0, layer_index);
		});

		drawComponent<RigidBodyComponent>(entity, [&](RigidBodyComponent &rigid_body) {
			UI::drawStruct(rigid_body, field_buttons);
		});

		drawComponent<BoxColliderComponent>(entity, [&](BoxColliderComponent &collider) {
			UI::drawStruct(collider, field_buttons);
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
			addComponentItems<ALL_COMPONENTS>(entity);
			ImGui::EndPopup();
		}
	} else if (asset_type && asset_type->isAuthored())
	{
		draw_serialized_asset(*asset_type, selected_path);
	} else if (!selected_path.empty())
	{
		auto &metadata = AssetManager::getMetadata(selected_path);
		if (metadata.isValid())
		{
			ImGui::SeparatorText(selected_path.filename().string().c_str());
			if (ImGui::SmallButton(ICON_FA_MAGNIFYING_GLASS " Show in Asset Browser"))
				asset_browser.setCurrentAsset(selected_path);

			UI::text("Asset Handle", "%llu", metadata.guid);
			UI::text("Runtime Handle", "%llu", metadata.runtimeGuid);

			static bool reimported = false;

			static std::filesystem::path last_selected_path;
			static bool settings_dirty = false;
			if (last_selected_path != selected_path)
			{
				last_selected_path = selected_path;
				settings_dirty = false;
			}

			const StructInfo *import_settings = metadata.type->importSettingsInfo;
			if (import_settings)
				settings_dirty |= UI::drawStruct(*import_settings, metadata.importSettings.data());

			if (metadata.type == AssetManager::getTypeInfo<RHITexture>())
			{
				auto texture = AssetManager::getTextureAsset(selected_path.string().c_str());

				static int mip_index = 0;
				if (reimported)
					mip_index = 0;
				UI::sliderInt("Mip", &mip_index, 0, texture->getDescription().mip_levels - 1);

				if (texture)
					draw_texture_preview(texture, mip_index, -1);
			}

			reimported = false;
			bool highlight_reimport = settings_dirty;
			if (highlight_reimport)
				ImGui::PushStyleColor(ImGuiCol_Button, UI::warning_color);
			if (ImGui::Button("Reimport", ImVec2(-FLT_MIN, 0)))
			{
				reimported = true;
				AssetManager::saveMetadata(metadata);
				AssetManager::reimport(selected_path);
				if (!AssetManager::hasValidRuntime(selected_path))
					AssetManager::recreateRuntime(selected_path);
				settings_dirty = false;
			}
			if (highlight_reimport)
				ImGui::PopStyleColor();
		}
	}

	for (entt::entity selected_id : context.getSelectedEntities())
	{
		Entity selected(selected_id);
		if (selected.hasComponent<LightComponent>())
		{
			auto &light = selected.getComponent<LightComponent>();
			glm::vec3 position = selected.getWorldTransformMatrix()[3];
			if (light.getType() == LIGHT_TYPE_POINT)
			{
				debug_renderer.addSphere(position, light.attenuation_radius, 32, glm::vec3(1, 1, 0));
			} else
			{
				float length = glm::distance(position, context.editor_camera.getPosition()) * 0.15f;
				debug_renderer.addArrow(position, position + selected.getLocalDirection(glm::vec3(0, 0, 1)) * length, length * 0.1f, glm::vec3(1, 1, 0), false);
			}
		}
	}

	ImGui::End();
	return false;
}
