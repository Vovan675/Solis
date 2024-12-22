#include "pch.h"
#include "ParametersPanel.h"
#include "Filesystem.h"
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"
#include "Model.h"


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

bool ParametersPanel::renderImGui(Entity entity, DebugRenderer &debug_renderer)
{
	ImGui::Begin((std::string(ICON_FA_PEN) + " Parameters###Parameters").c_str());
	bool is_using_ui = ImGui::IsWindowFocused();
	if (entity)
	{
		drawComponent<MeshRendererComponent>(entity, "Mesh Renderer", [&](MeshRendererComponent &mesh_renderer) {
			for (auto &mesh_id : mesh_renderer.meshes)
			{
				auto mesh = mesh_id.getMesh();
				debug_renderer.addBoundBox(mesh->bound_box * entity.getWorldTransformMatrix());
			}

			ImGui::SeparatorText("Mesh Settings");
			if (mesh_renderer.meshes.empty())
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "No mesh");

			// TODO: Select mesh (one piece of already loaded model asset)
			if (ImGui::Button("Select mesh"))
			{
				std::string path = Filesystem::openFileDialog();
				if (!path.empty())
				{
					auto model = AssetManager::getModelAsset(path);
					mesh_renderer.meshes.clear();
					auto &linear_nodes = model->getLinearNodes();
					for (auto node : linear_nodes)
					{
						if (!node->meshes.empty())
						{
							mesh_renderer.setFromMeshNode(model, node);
							break;
						}
					}
				}
			}

			ImGui::SeparatorText("Materials");
			for (int i = 0; i < mesh_renderer.materials.size(); i++)
			{
				auto mat = mesh_renderer.materials[i];
				std::string name = "Material " + i;
				if (ImGui::TreeNode(name.c_str()))
				{
					{
						bool use_texture = mat->albedo_tex_id >= 0;
						if (ImGui::Checkbox("Use albedo texture", &use_texture))
						{
							mat->albedo_tex_id = use_texture ? 0 : -1;
						}
						if (use_texture)
						{
							if (ImGui::Button("Select"))
							{
								std::string path = Filesystem::openFileDialog();
								if (!path.empty())
								{
									auto texture = AssetManager::getTextureAsset(path);
									mat->albedo_tex_id = BindlessResources::addTexture(texture.get());
								}
							}

							// TODO: make for all, unify using struct for payload
							if (ImGui::BeginDragDropTarget())
							{
								if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH", ImGuiDragDropFlags_AcceptPeekOnly))
								{
									const char *payload_str = (const char *)payload->Data;
									std::string extension = std::filesystem::path(payload_str).extension().string();
									if (extension == ".png" || extension == ".jpg")
									{
										if (payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH"))
										{
											auto texture = AssetManager::getTextureAsset(payload_str);
											mat->albedo_tex_id = BindlessResources::addTexture(texture.get());
										}
									}
								}
								ImGui::EndDragDropTarget();
							}
						} else
						{
							ImGui::ColorEdit4("Color", mat->albedo.data.data);
						}
					}

					{
						bool use_texture = mat->normal_tex_id >= 0;
						if (ImGui::Checkbox("Use normal texture", &use_texture))
						{
							mat->normal_tex_id = use_texture ? 0 : -1;
						}
						if (use_texture)
						{
							if (ImGui::Button("Select"))
							{
								std::string path = Filesystem::openFileDialog();
								if (!path.empty())
								{
									TextureDescription tex_description{};
									tex_description.image_format = VK_FORMAT_R8G8B8A8_UNORM;
									tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;
									auto texture = AssetManager::getTextureAsset(path);
									mat->normal_tex_id = BindlessResources::addTexture(texture.get());
								}
							}
						}
					}
					// TODO: edit all textures

					ImGui::DragFloat("Metalness", &mat->metalness, 0.1, 0, 1.0);
					ImGui::DragFloat("Roughness", &mat->roughness, 0.1, 0, 1.0);
					ImGui::DragFloat("Specular", &mat->specular, 0.1, 0, 1.0);
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
			ImGui::SliderFloat("Light Radius", &light.radius, 0.001f, 40.0);
			ImGui::SliderFloat("Light Intensity", &light.intensity, 0.01f, 25);
		});


		if (centeredButton("Add component..."))
			ImGui::OpenPopup("add_component_popup");
		if (ImGui::BeginPopup("add_component_popup"))
		{
			addComponentButton<MeshRendererComponent>(entity, "Mesh Renderer");
			addComponentButton<LightComponent>(entity, "Light");
			ImGui::EndPopup();
		}
	}
	ImGui::End();
	return false;
}
