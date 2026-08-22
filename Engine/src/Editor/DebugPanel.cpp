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

	if (UI::beginSection("Visualization", true))
	{
		UI::convar(render_debug_rendering.getDescription());

		if (render_debug_rendering)
		{
			const char *items[] = {"All", "Final Composite", "Albedo", "Metalness", "Roughness", "Specular", "Normal", "Depth", "Position", "Light Diffuse", "Light Specular", "BRDF LUT", "SSAO", "DDGI", "HiZ", "Debug Texture", "Overdraw", "Motion Vectors"};
			int mode = render_debug_rendering_mode;
			if (UI::combo("Preview", &mode, items, IM_ARRAYSIZE(items)))
				render_debug_rendering_mode = mode;
			debug_renderer->ubo.present_mode = render_debug_rendering_mode;
		}

		UI::convar(render_ddgi_visualize.getDescription());
		if (render_ddgi_visualize)
		{
			const char *items[] = {"Irradiance", "Distance", "State", "State Not Disabled", "Cascades"};
			int mode = render_ddgi_visualize_mode;
			if (UI::combo("DDGI Visualize Mode", &mode, items, IM_ARRAYSIZE(items)))
				render_ddgi_visualize_mode = mode;
		}

		UI::convar(render_lighting_only.getDescription());
		UI::convar(render_culling_freeze.getDescription());
		UI::convar(render_culling_hiz_debug.getDescription());
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

		uint64_t geom_used = GlobalBufferCache::getMeshletGeometryBufferUsedSize();
		uint64_t geom_max = GlobalBufferCache::getMeshletGeometryBufferMaxSize();
		UI::text("Meshlet Geometry", "%.1f / %.0f MB", toMB(geom_used), toMB(geom_max));

		UI::property("Usage", [&]
		{
			ImGui::ProgressBar(geom_max > 0 ? (float)geom_used / geom_max : 0.0f, ImVec2(-FLT_MIN, 0));
			return false;
		});

		if (geometry_streaming)
		{
			const auto &s = geometry_streaming->getStats();
			ImGui::SeparatorText("Streaming");
			float resident_fraction = s.total_groups > 0 ? (float)s.resident_groups / s.total_groups : 0.0f;
			UI::text("Resident Groups", "%u / %u (%.1f%%)", s.resident_groups, s.total_groups, resident_fraction * 100.0f);
			UI::text("Meshes Registered", "%u", s.registered_mesh_count);
			UI::text("Pending Loads", "%u", s.pending_load_queue_size);
			UI::text("Pending Frees", "%u", s.pending_frees_count);

			ImGui::SeparatorText("This Frame");
			UI::text("Loads", "%u (%.2f MB)", s.loads_last_frame, toMB(s.bytes_loaded_last_frame));
			UI::text("Unloads", "%u (%.2f MB)", s.unloads_last_frame, toMB(s.bytes_unloaded_last_frame));

			ImGui::SeparatorText("Cumulative");
			UI::text("Loads", "%llu (%.1f MB)", s.total_loads, toMB(s.total_bytes_loaded));
			UI::text("Unloads", "%llu (%.1f MB)", s.total_unloads, toMB(s.total_bytes_unloaded));
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

	ImGui::End();
}
