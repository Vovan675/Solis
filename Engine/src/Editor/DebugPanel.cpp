#include "pch.h"
#include "DebugPanel.h"
#include "UI.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalBufferCache.h"
#include "Core/Variables.h"

// Test reflection and serialized types
inline const char *const reflection_test_mode_items[] = {"Linear", "Constant"};

struct ReflectionTestGroup
{
	bool enabled = false;
	glm::vec3 color = glm::vec3(0.5f, 0.6f, 0.7f);
	float density = 0.02f;
	float falloff = 1.0f;
	bool high_quality = false;
	int samples = 8;
	int mode = 0;
};

REFLECT_BEGIN(ReflectionTestGroup)
	REFLECT_FIELD(enabled).label("Enable Group"),
	REFLECT_FIELD(color).range(0.0f, 1.0f).format("%.2f").EDIT_IF(owner.enabled),
	REFLECT_CATEGORY("Shape"),
	REFLECT_FIELD(density).range(0.0f, 1.0f).format("%.3f").logarithmic().EDIT_IF(owner.enabled),
	REFLECT_CATEGORY("Shape - Advanced"),
	REFLECT_FIELD(high_quality).EDIT_IF(owner.enabled),
	REFLECT_FIELD(samples).range(1.0f, 64.0f).EDIT_IF(owner.enabled && owner.high_quality),
	REFLECT_CATEGORY("Shape"),
	REFLECT_FIELD(falloff).range(0.1f, 4.0f).format("%.2f").EDIT_IF(owner.enabled),
	REFLECT_CATEGORY("Debug"),
	REFLECT_FIELD(mode).items(reflection_test_mode_items).radio().EDIT_IF(owner.enabled),
REFLECT_END()

struct ReflectionTestChildAsset : Asset
{
	AssetReference texture;
	float coverage = 0.4f;
	float altitude = 2000.0f;
};

REFLECT_BEGIN(ReflectionTestChildAsset)
	REFLECT_FIELD(texture).asset<RHITexture>(),
	REFLECT_FIELD(coverage).range(0.0f, 1.0f).format("%.2f"),
	REFLECT_FIELD(altitude).range(100.0f, 12000.0f).format("%.0f m").logarithmic(),
REFLECT_END()

struct ReflectionTestAsset : Asset
{
	AssetReference hdri;
	float intensity = 15000.0f;
	ReflectionTestGroup group;
	AssetReference inherit_from;
	AssetReference child;
	eastl::vector<ReflectionTestGroup> group_array;
	eastl::vector<AssetReference> texture_array;
};

REFLECT_BEGIN(ReflectionTestAsset)
	REFLECT_FIELD(hdri).label("HDRI").asset<RHITexture>(),
	REFLECT_FIELD(intensity).range(1.0f, 100000.0f).format("%.0f nits").logarithmic(),
	REFLECT_FIELD(group),
	REFLECT_FIELD(inherit_from).asset<ReflectionTestAsset>(),
	REFLECT_FIELD(child).asset<ReflectionTestChildAsset>(),
	REFLECT_CATEGORY("Arrays"),
	REFLECT_FIELD(group_array),
	REFLECT_FIELD(texture_array).asset<RHITexture>(),
REFLECT_END()

static const AssetTypeInfo *registered_test_asset_type = AssetManager::registerSerializedType<ReflectionTestAsset>(".reflectiontest");
static const AssetTypeInfo *registered_test_child_asset_type = AssetManager::registerSerializedType<ReflectionTestChildAsset>(".reflectiontestchild");

template<typename T>
static void collect_cvars(eastl::vector<ConVarDescription *> &out)
{
	for (ConVar<T> &cvar : ConVarSystem::getCVars<T>())
		if ((cvar.description.flags & CON_VAR_FLAG_HIDDEN) == 0)
			out.push_back(&cvar.description);
}

static void draw_all_cvars()
{
	eastl::vector<ConVarDescription *> con_vars;
	collect_cvars<int>(con_vars);
	collect_cvars<float>(con_vars);
	collect_cvars<bool>(con_vars);
	collect_cvars<eastl::string>(con_vars);

	eastl::sort(con_vars.begin(), con_vars.end(), [](ConVarDescription *a, ConVarDescription *b)
	{
		return a->name < b->name;
	});

	for (ConVarDescription *cvar : con_vars)
		UI::convar(cvar);
}

void DebugPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((eastl::string(ICON_FA_BUG) + " Debug Window###Debug Window").c_str());

	UI::text("RHI", "%s", gDynamicRHI->getName());

	const eastl::vector<DynamicRHI::GPUTiming> &gpu_timings = gDynamicRHI->getGPUTimings();
	float total_gpu_ms = 0;
	average_gpu_timings.resize(gpu_timings.size());
	for (int i = 0; i < gpu_timings.size(); i++)
	{
		DynamicRHI::GPUTiming &average = average_gpu_timings[i];
		if (average.name != gpu_timings[i].name)
			average = gpu_timings[i];
		else
			average.milliseconds = glm::mix(average.milliseconds, gpu_timings[i].milliseconds, 0.05f);
		total_gpu_ms += average.milliseconds;
	}

	if (UI::beginSection("GPU Passes"))
	{
		UI::sliderFloat("Threshold", &gpu_timing_threshold, 0.0f, 1.0f, "%.2f ms");
		UI::text("Total", "%.2f ms", total_gpu_ms);
		if (ImGui::BeginTable("##gpu_passes", 2, ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("000.00 ms").x);
			for (const auto &timing : average_gpu_timings)
			{
				if (timing.milliseconds < gpu_timing_threshold)
					continue;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(timing.name.c_str());
				ImGui::SetItemTooltip("%s", timing.name.c_str());

				ImGui::TableNextColumn();
				char time[16];
				snprintf(time, sizeof(time), "%.2f ms", timing.milliseconds);
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(time).x);
				ImGui::TextUnformatted(time);
			}
			ImGui::EndTable();
		}
		UI::endSection();
	}

	if (UI::beginSection("Visualization", true))
	{
		UI::convar(render_debug_rendering.getDescription());

		if (render_debug_rendering)
		{
			const char *items[] = {"All", "Final Composite", "Albedo", "Metalness", "Roughness", "Specular", "Emissive", "Normal", "Depth", "Position", "Light Diffuse", "Light Specular", "BRDF LUT", "Ambient Occlusion", "DDGI", "HiZ", "Debug Texture", "Overdraw", "Motion Vectors"};
			int mode = render_debug_rendering_mode;
			if (UI::combo("Preview", &mode, items, IM_ARRAYSIZE(items)))
				render_debug_rendering_mode = mode;
			debug_renderer->ubo.present_mode = render_debug_rendering_mode;
		}

		UI::convar(render_ddgi_visualize.getDescription());
		if (render_ddgi_visualize)
		{
			const char *items[] = {"Irradiance", "Distance", "State", "State Not Disabled", "Cascades", "Relocation"};
			int mode = render_ddgi_visualize_mode;
			if (UI::combo("DDGI Visualize Mode", &mode, items, IM_ARRAYSIZE(items)))
				render_ddgi_visualize_mode = mode;
		}
		UI::convar(render_lighting_only.getDescription());
		UI::convar(render_culling_freeze.getDescription());
		UI::convar(render_culling_hiz_debug.getDescription());
		UI::convar(render_meshlets_lod_error_pixels.getDescription());
		UI::convar(render_meshlets_bvh_visualize.getDescription());
		ImGui::BeginDisabled(!render_meshlets_bvh_visualize);
		UI::convar(render_meshlets_bvh_visualize_depth.getDescription());
		ImGui::EndDisabled();
		UI::endSection();
	}

	mitsuba_bridge->renderImGui(context);

	if (UI::beginSection("Geometry Buffers"))
	{
		auto toMB = [](uint64_t bytes) { return bytes / (1024.0f * 1024.0f); };

		if (geometry_streaming)
		{
			const auto &s = geometry_streaming->getStats();
			float pool_slots = eastl::max(s.pool_slots, 1u);
			uint64_t root_capacity = GlobalBufferCache::getMeshletGeometryBufferMaxSize() - s.pool_slots * MESHLET_PAGE_SIZE;
			ImGui::SeparatorText("Streaming");
			UI::text("Root Pages", "%u meshes, %.1f / %.0f MB", s.registered_mesh_count, toMB(s.root_bytes), toMB(root_capacity));
			UI::text("Pool", "%u / %u pages", s.used_slots, s.pool_slots);
			UI::property("Pool Usage", [&]
			{
				ImGui::ProgressBar(s.used_slots / pool_slots, ImVec2(-FLT_MIN, 0));
				return false;
			});
			UI::text("Needed Pages", "%u (%.0f%% of pool)", s.needed_pages, 100.0f * s.needed_pages / pool_slots);

			ImGui::SeparatorText("Last Readback");
			UI::text("Missing Pages", "%u", s.missing_pages);
			UI::text("Loaded Pages", "%u", s.loaded_pages);
		}
		UI::endSection();
	}

	if (UI::beginSection("Debug Info"))
	{
		auto info = Renderer::getDebugInfo();
		UI::text("Descriptors", "%u", info.descriptors_count);
		UI::text("Descriptor Bindings", "%u", info.descriptor_bindings_count);
		UI::text("Descriptors Max Offset", "%u", info.descriptors_max_offset);
		UI::text("Draw Calls", "%u", info.drawcalls);
		UI::endSection();
	}

	if (UI::beginSection("Console Variables"))
	{
		draw_all_cvars();
		UI::endSection();
	}

	static bool show_imgui_demo = false;
	UI::checkbox("Show ImGUI Demo", &show_imgui_demo);
	if (show_imgui_demo)
		ImGui::ShowDemoWindow();

	static bool show_imgui_style_editor = false;
	UI::checkbox("Show ImGUI Style Editor", &show_imgui_style_editor);
	if (show_imgui_style_editor)
		ImGui::ShowStyleEditor();

	ImGui::End();
}
