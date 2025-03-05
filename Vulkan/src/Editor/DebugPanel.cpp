#include "pch.h"
#include "DebugPanel.h"
#include "imgui/IconsFontAwesome6.h"
#include "Rendering/Renderer.h"
#include "Variables.h"

void DebugPanel::renderImGui(EditorContext context)
{
	ImGui::Begin((std::string(ICON_FA_CUBES) + " Debug Window###Debug Window").c_str());

	ImGui::Text("RHI: %s", + gDynamicRHI->getName());

	if (ImGui::Button("Recompile shaders"))
	{
		// Wait for all operations complete
		//vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
		//Shader::recompileAllShaders(); // TODO: implement
	}

	if (ImGui::TreeNode("Debug Info"))
	{
		auto info = Renderer::getDebugInfo();
		ImGui::Text("descriptors_count: %u", info.descriptors_count);
		ImGui::Text("descriptor_bindings_count: %u", info.descriptor_bindings_count);
		ImGui::Text("descriptors_max_offset: %u", info.descriptors_max_offset);
		ImGui::Text("Draw Calls: %u", info.drawcalls);
		ImGui::TreePop();
	}

	ConVarSystem::drawConVarImGui(render_debug_rendering.getDescription());

	if (render_debug_rendering)
	{
		int mode = render_debug_rendering_mode;
		char* items[] = { "All", "Final Composite", "Albedo", "Metalness", "Roughness", "Specular", "Normal", "Depth", "Position", "Light Diffuse", "Light Specular", "BRDF LUT", "SSAO" };
		if (ImGui::BeginCombo("Preview Combo", items[mode]))
		{
			for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{
				bool is_selected = (mode == n);
				if (ImGui::Selectable(items[n], is_selected))	
					render_debug_rendering_mode = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		debug_renderer->ubo.present_mode = render_debug_rendering_mode;
	}

	ConVarSystem::drawImGui();

	auto &camera = context.editor_camera;
	float cam_speed = camera->getSpeed();
	if (ImGui::SliderFloat("Camera Speed", &cam_speed, 0.1f, 10.0f))
	{
		camera->setSpeed(cam_speed);
		camera->updateMatrices();
	}

	float cam_near = camera->getNear();
	if (ImGui::SliderFloat("Camera Near", &cam_near, 0.01f, 3.5f))
	{
		camera->setNear(cam_near);
		camera->updateMatrices();
	}

	float cam_far = camera->getFar();
	if (ImGui::SliderFloat("Camera Far", &cam_far, 1.0f, 300.0f))
	{
		camera->setFar(cam_far);
		camera->updateMatrices();
	}

	glm::vec3 cam_pos = camera->getPosition();
	ImGui::InputFloat3("Camera Position", cam_pos.data.data);

	post_renderer->renderImgui();
	ssao_renderer->renderImgui();
	defferred_lighting_renderer->renderImgui();

	sky_renderer->renderImgui();
	if (render_automatic_sun_position)
	{
		auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
		for (const auto &entity_id : entities_id)
		{
			Entity entity(entity_id);

			LightComponent &light = entity.getComponent<LightComponent>();
			if (light.getType() != LIGHT_TYPE_DIRECTIONAL)
				continue;
			sky_renderer->procedural_uniforms.sun_direction = entity.getLocalDirection(glm::vec3(0, 0, -1));
			break;
		}
	}

}
