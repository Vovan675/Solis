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

bool ViewportPanel::renderImGui(EditorContext &context, float delta_time, bool fullscreen)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (fullscreen)
	{
		ImGuiViewport *main_viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(main_viewport->Pos);
		ImGui::SetNextWindowSize(main_viewport->Size);
		ImGui::SetNextWindowViewport(main_viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::Begin("Fullscreen Viewport", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
		ImGui::PopStyleVar(2);
	} else
	{
		ImGui::Begin((eastl::string(ICON_FA_EXPAND) + " Viewport###Viewport").c_str());
	}
	ImGui::PopStyleVar();

	bool is_hovered = ImGui::IsWindowHovered();

	ImVec2 viewport_size = ImGui::GetContentRegionAvail();
	ImVec2 output_resolution = viewport_size;
	if (render_virtual_width > 0 && render_virtual_height > 0)
	{
		output_resolution = ImVec2(render_virtual_width, render_virtual_height);
		float scale = glm::min(viewport_size.x / output_resolution.x, viewport_size.y / output_resolution.y);
		ImVec2 cursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cursor.x + (viewport_size.x - output_resolution.x * scale) * 0.5f, cursor.y + (viewport_size.y - output_resolution.y * scale) * 0.5f));
		viewport_size = ImVec2(output_resolution.x * scale, output_resolution.y * scale);
	}

	if (!viewport_texture || output_resolution.x != viewport_texture->getWidth() || output_resolution.y != viewport_texture->getHeight())
	{
		TextureDescription description;
		description.width = output_resolution.x;
		description.height = output_resolution.y;

		description.format = FORMAT_R8G8B8A8_UNORM;
		description.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_SRC;
		description.sampler_mode = SAMPLER_MODE_REPEAT;
		
		viewport_texture = gDynamicRHI->createTexture(description);
		viewport_texture->fill();
		viewport_texture->setDebugName("Viewport Texture");
	}

	ImGui::Image(ImGuiWrapper::getTextureId(viewport_texture), viewport_size);
	ImVec2 viewport_pos = ImGui::GetItemRectMin();

	Renderer::setOutputResolution({output_resolution.x, output_resolution.y});
	context.editor_camera.setAspect(output_resolution.x / output_resolution.y);

	// ImGuizmo
	Entity selected_entity = context.getSelectedEntity();
	if (selected_entity && !fullscreen)
	{
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewport_pos.x, viewport_pos.y, viewport_size.x, viewport_size.y);

		Camera &camera = context.editor_camera;
		glm::mat4 projection = glm::perspective(glm::radians(camera.getFov()), camera.getAspect(), camera.getNear(), camera.getFar());

		bool snap = gInput.isKeyDown(GLFW_KEY_LEFT_CONTROL);
		glm::vec3 snap_values;
		if (guizmo_tool_type == ImGuizmo::ROTATE)
			snap_values = glm::vec3(15.0f);
		else if (guizmo_tool_type == ImGuizmo::SCALE)
			snap_values = glm::vec3(0.1f);
		else
			snap_values = glm::vec3(0.5f);

		eastl::vector<entt::entity> roots;
		// If parent selected, move only parent
		for (entt::entity id : context.selected_entities)
		{
			Entity parent = Entity(id).getParent();
			while (parent && !context.isSelected(parent))
				parent = parent.getParent();
			if (!parent)
				roots.push_back(id);
		}

		bool was_using = ImGuizmo::IsUsing();
		if (!was_using)
		{
			// Use center as pivot point
			glm::vec3 center = glm::vec3(0.0f);
			for (entt::entity id : roots)
				center += glm::vec3(Entity(id).getWorldTransformMatrix()[3]);
			center /= (float)roots.size();

			glm::vec3 position;
			glm::vec3 scale;
			glm::quat rotation;
			Math::decomposeTransform(selected_entity.getWorldTransformMatrix(), position, rotation, scale);
			guizmo_pivot = glm::translate(glm::mat4(1.0f), center) * glm::toMat4(rotation);
		}
		glm::mat4 old_pivot = guizmo_pivot;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::MODE mode = guizmo_tool_type == ImGuizmo::SCALE ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
		bool changed = ImGuizmo::Manipulate(glm::value_ptr(camera.getView()), glm::value_ptr(projection), guizmo_tool_type, mode, glm::value_ptr(guizmo_pivot), nullptr, snap ? glm::value_ptr(snap_values) : nullptr);

		if (!was_using && ImGuizmo::IsUsing() && gInput.isKeyDown(GLFW_KEY_LEFT_SHIFT))
		{
			for (entt::entity &id : roots)
				id = Scene::getCurrentScene()->duplicateEntity(id);
			context.selected_entities = roots;
		}

		if (changed)
		{
			glm::mat4 delta = guizmo_pivot * glm::inverse(old_pivot);
			for (entt::entity id : roots)
			{
				TransformComponent &transform_component = Entity(id).getTransform();
				transform_component.setWorldTransform(delta * transform_component.getWorldTransform());
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
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (!fullscreen)
		drawStatsBar(delta_time, viewport_pos);
	ImGui::End();
	return is_hovered;
}

void ViewportPanel::update()
{
	if (ImGui::GetIO().WantTextInput)
		return;

	if (gInput.isKeyPressed(GLFW_KEY_R))
		guizmo_tool_type = ImGuizmo::ROTATE;
	if (gInput.isKeyPressed(GLFW_KEY_T))
		guizmo_tool_type = ImGuizmo::TRANSLATE;
	if (gInput.isKeyPressed(GLFW_KEY_Y))
		guizmo_tool_type = ImGuizmo::SCALE;
}
