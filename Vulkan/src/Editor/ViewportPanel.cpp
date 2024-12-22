#include "pch.h"
#include "ViewportPanel.h"
#include "imgui/IconsFontAwesome6.h"
#include "imgui/ImGuiWrapper.h"
#include "Rendering/Renderer.h"
#include "GuiUtils.h"
#include <glm/gtc/type_ptr.hpp>
#include "Application.h"

bool ViewportPanel::renderImGui(EditorContext context, float delta_time)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin((std::string(ICON_FA_EXPAND) + " Viewport###Viewport").c_str());
	ImGui::PopStyleVar();

	bool is_viewport_focused = ImGui::IsWindowFocused();

	ImVec2 viewport_offset = ImGui::GetCursorPos();
	ImVec2 viewport_pos = ImGui::GetWindowPos();
	viewport_pos.x += viewport_offset.x;
	viewport_pos.y += viewport_offset.y;

	auto &viewport_texture = Renderer::getRenderTarget(RENDER_TARGET_FINAL);
		
	ImVec2 viewport_size = ImGui::GetContentRegionAvail();
	if (viewport_texture)
		ImGui::Image(ImGuiWrapper::getTextureDescriptorSet(viewport_texture), viewport_size);

	Renderer::setViewportSize({viewport_size.x, viewport_size.y});
	context.editor_camera->setAspect(viewport_size.x / viewport_size.y);

	// ImGuizmo
	Entity &selected_entity = context.selected_entity;
	if (selected_entity)
	{
		auto &transform_component = selected_entity.getTransform();

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewport_pos.x, viewport_pos.y, viewport_size.x, viewport_size.y);

		glm::mat4 proj = context.editor_camera->getProj();
		proj[1][1] *= -1.0f;

		glm::mat4 delta_transform;
		glm::mat4 transform = selected_entity.getWorldTransformMatrix();

		ImGuizmo::SetOrthographic(false);
		if (ImGuizmo::Manipulate(glm::value_ptr(context.editor_camera->getView()), glm::value_ptr(proj), guizmo_tool_type, ImGuizmo::WORLD, glm::value_ptr(transform), glm::value_ptr(delta_transform)))
		{
			//
			glm::vec3 dposition, drotation, dscale;
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(delta_transform), glm::value_ptr(dposition), glm::value_ptr(drotation), glm::value_ptr(dscale));

			glm::mat3 new_transform;
			glm::vec3 position, rotation, scale;
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(new_transform), glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale));

			//ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale), glm::value_ptr(transform));
			//

			if (transform_component.parent != entt::null)
			{
				transform_component.setTransform(inverse(selected_entity.getParent().getTransform().getTransformMatrix()) * transform);
			} else
			{
				transform_component.setTransform(transform);
			}
		}
	}

	GuiUtils::draw_stats_bar(delta_time, viewport_pos);
	ImGui::End();
	return is_viewport_focused;
}

void ViewportPanel::update()
{
	if (input.isKeyDown(GLFW_KEY_R))
		guizmo_tool_type = ImGuizmo::ROTATE;
	if (input.isKeyDown(GLFW_KEY_T))
		guizmo_tool_type = ImGuizmo::TRANSLATE;
	if (input.isKeyDown(GLFW_KEY_Y))
		guizmo_tool_type = ImGuizmo::SCALE;
}
