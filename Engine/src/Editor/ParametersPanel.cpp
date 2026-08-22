#include "pch.h"
#include "ParametersPanel.h"
#include "UI.h"
#include "Rendering/Model.h"
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
	bool open = ImGui::CollapsingHeader(title.c_str(), &close, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding);
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

bool ParametersPanel::renderImGui(EditorContext &context, DebugRenderer &debug_renderer, AssetBrowserPanel &asset_browser)
{
	ImGui::Begin((eastl::string(ICON_FA_PEN) + " Parameters###Parameters").c_str());

	auto selected_path = context.selected_path;
	const AssetTypeInfo *asset_type = AssetManager::findTypeInfoByExtension(selected_path.extension().string().c_str());
	Entity entity = context.selected_entity;
	bool prefer_asset = context.selection_type == EditorSelectionType::Asset && !selected_path.empty();
	if (entity && !prefer_asset)
	{
		drawComponent<TransformComponent>(entity, [&](TransformComponent &transform_component) {
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

		drawComponent<MeshRendererComponent>(entity, [&](MeshRendererComponent &mesh_renderer)
		{
			if (mesh_renderer.meshes.empty())
				ImGui::TextColored(ImVec4(0.90f, 0.35f, 0.30f, 1.0f), "No mesh");

			AssetReference model_reference;
			if (!mesh_renderer.meshes.empty())
				model_reference = mesh_renderer.meshes[0].model_asset;

			if (UI::assetField("Mesh", &model_reference, AssetManager::getTypeInfo<Model>()))
			{
				mesh_renderer.meshes.clear();
				Ref<Model> model = AssetManager::getAsset<Model>(model_reference);
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

			ImGui::SeparatorText("Materials");
			for (int i = 0; i < mesh_renderer.meshes.size(); i++)
			{
				ImGui::PushID(i);
				eastl::string slot_name = "Slot " + eastl::to_string(i);

				AssetReference slot_asset;
				if (i < mesh_renderer.materials.size())
					slot_asset = mesh_renderer.materials[i].material_asset;

				if (UI::assetField(slot_name.c_str(), &slot_asset, AssetManager::getTypeInfo<Material>()))
				{
					mesh_renderer.setMaterial(i, AssetManager::getAsset<Material>(slot_asset));
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

		drawComponent<LightComponent>(entity, [](LightComponent &light) {
			UI::drawStruct(light);

			RHITextureRef texture = light.getShadowMap();
			if (!texture)
				return;

			ImGui::SeparatorText("Shadow Map");
			static int layer_index = 0;
			UI::sliderInt("Layer / Face", &layer_index, 0, light.getType() == LIGHT_TYPE_POINT ? 5 : 3);
			draw_texture_preview(texture, 0, layer_index);
		});

		drawComponent<RigidBodyComponent>(entity, [](RigidBodyComponent &rigid_body) {
			UI::drawStruct(rigid_body);
		});

		drawComponent<BoxColliderComponent>(entity, [&](BoxColliderComponent &collider) {
			UI::drawStruct(collider);
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
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
			if (ImGui::Button("Reimport", ImVec2(-FLT_MIN, 0)))
			{
				reimported = true;
				AssetManager::saveMetadata(metadata);
				AssetManager::reimport(selected_path);
				settings_dirty = false;
			}
			if (highlight_reimport)
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
