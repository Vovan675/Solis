#include "pch.h"
#include "ParametersPanel.h"
#include "Filesystem.h"
#include "imgui.h"


bool ParametersPanel::renderImGui(Entity entity, std::shared_ptr<DebugRenderer> debug_renderer)
{
	ImGui::Begin("Parameters");
	bool is_using_ui = ImGui::IsWindowFocused();
	if (entity)
	{
		if (entity.hasComponent<MeshRendererComponent>())
		{
			auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
			for (auto &mesh_id : mesh_renderer.meshes)
			{
				auto mesh = mesh_id.getMesh();
				debug_renderer->addBoundBox(mesh->bound_box * entity.getWorldTransformMatrix());
			}
			ImGui::Text("Materials");
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
									tex_description.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
									tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
									tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
									auto texture = AssetManager::getTextureAsset(path);
									mat->normal_tex_id = BindlessResources::addTexture(texture.get());
								}
							}
						}
					}
					// TODO: edit all textures

					ImGui::TreePop();
				}
			}
		}

		if (entity.hasComponent<LightComponent>())
		{
			auto &light = entity.getComponent<LightComponent>();
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
			ImGui::SliderFloat3("Light Color", light.color.data.data, 0, 1);
			ImGui::SliderFloat("Light Radius", &light.radius, 0.001f, 40.0);
			ImGui::SliderFloat("Light Intensity", &light.intensity, 0.01f, 25);
		}
	}
	ImGui::End();
	return false;
}
