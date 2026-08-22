#include "pch.h"
#include "ViewportPanel.h"
#include "imgui/IconsFontAwesome6.h"
#include "imgui/ImGuiWrapper.h"
#include "Rendering/Renderer.h"
#include "UI.h"
#include <glm/gtc/type_ptr.hpp>
#include "Application.h"
#include "Rendering/Model.h"
#include "Assets/AssetManager.h"

static void drawStatsBar(float delta_time, ImVec2 pos)
{
	ImVec4 background(22.0f / 255.0f, 22.0f / 255.0f, 22.0f / 255.0f, 0.5f);
	ImGui::SetNextWindowPos(pos);
	ImGui::Begin("Stats Bar", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);

	ImGui::PushFont(UI::font_small);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 5));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 2));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, background);

	ImGui::BeginChild("FPS", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);

	static float last_fps = 0;
	static float last_fps_timer = 0.3f;
	last_fps_timer -= delta_time;
	if (last_fps_timer <= 0)
	{
		last_fps_timer = 0.3f;
		last_fps = 1.0f / delta_time;
	}

	glm::ivec2 render_resolution = Renderer::getRenderResolution();
	glm::ivec2 output_resolution = Renderer::getOutputResolution();
	const auto &stats = gDynamicRHI->getGPUStatistics();

	ImGui::Text("FPS: %i (%.2f ms)", (int)last_fps, 1.0f / last_fps * 1000);
	ImGui::Text("Render: %i x %i", render_resolution.x, render_resolution.y);
	ImGui::Text("Output: %i x %i", output_resolution.x, output_resolution.y);
	ImGui::Text("Frame: %i", gDynamicRHI->getFrame());
	ImGui::Text("Triangles: %llu", stats.clipping_primitives);
	ImGui::Text("Vertices: %llu", stats.vertex_shader_invocations + stats.mesh_shader_invocations);
	ImGui::Text("Vertices (Mesh): %llu", stats.mesh_shader_invocations);

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
	ImGui::PopFont();

	ImGui::End();
}

bool ViewportPanel::renderImGui(EditorContext &context, float delta_time)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin((eastl::string(ICON_FA_EXPAND) + " Viewport###Viewport").c_str());
	ImGui::PopStyleVar();

	bool is_viewport_focused = ImGui::IsWindowFocused();

	ImVec2 viewport_offset = ImGui::GetCursorPos();
	ImVec2 viewport_pos = ImGui::GetWindowPos();
	viewport_pos.x += viewport_offset.x;
	viewport_pos.y += viewport_offset.y;

	ImVec2 viewport_size = ImGui::GetContentRegionAvail();

	if (!viewport_texture || viewport_size.x != viewport_texture->getWidth() || viewport_size.y != viewport_texture->getHeight())
	{
		TextureDescription description;
		description.width = viewport_size.x;
		description.height = viewport_size.y;

		description.format = FORMAT_R8G8B8A8_UNORM;
		description.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		description.sampler_mode = SAMPLER_MODE_REPEAT;
		
		viewport_texture = gDynamicRHI->createTexture(description);
		viewport_texture->fill();
		viewport_texture->setDebugName("Viewport Texture");
	}

	if (viewport_texture)
		ImGui::Image(ImGuiWrapper::getTextureId(viewport_texture), viewport_size);

	Renderer::setOutputResolution({viewport_size.x, viewport_size.y});
	context.editor_camera.setAspect(viewport_size.x / viewport_size.y);

	// ImGuizmo
	Entity &selected_entity = context.selected_entity;
	if (selected_entity)
	{
		auto &transform_component = selected_entity.getTransform();

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewport_pos.x, viewport_pos.y, viewport_size.x, viewport_size.y);

		glm::mat4 proj = context.editor_camera.getProj();

		glm::mat4 delta_transform;
		glm::mat4 transform = selected_entity.getWorldTransformMatrix();

		ImGuizmo::SetOrthographic(false);
		if (ImGuizmo::Manipulate(glm::value_ptr(context.editor_camera.getView()), glm::value_ptr(proj), guizmo_tool_type, ImGuizmo::WORLD, glm::value_ptr(transform), glm::value_ptr(delta_transform)))
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
				transform_component.setWorldTransform(transform);
				//transform_component.setLocalTransform(inverse(selected_entity.getParent().getTransform().getLocalTransform()) * transform);
			} else
			{
				transform_component.setWorldTransform(transform);
				//transform_component.setLocalTransform(transform);
			}
		}
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH", ImGuiDragDropFlags_AcceptPeekOnly))
		{
			const char *payload_str = (const char *)payload->Data;
			eastl::string extension = std::filesystem::path(payload_str).extension().string().c_str();
			if (AssetManager::findTypeInfoByExtension(extension) == AssetManager::getTypeInfo<Model>())
			{
				if (payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH"))
				{
					auto model = AssetManager::getModelAsset(payload_str);
					Entity entity = model->createEntity(model);
					entity.getTransform().setLocalScale(glm::vec3(0.01));
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	drawStatsBar(delta_time, viewport_pos);
	ImGui::End();
	return is_viewport_focused;
}

void ViewportPanel::update()
{
	if (gInput.isKeyDown(GLFW_KEY_R))
		guizmo_tool_type = ImGuizmo::ROTATE;
	if (gInput.isKeyDown(GLFW_KEY_T))
		guizmo_tool_type = ImGuizmo::TRANSLATE;
	if (gInput.isKeyDown(GLFW_KEY_Y))
		guizmo_tool_type = ImGuizmo::SCALE;
}
