#include "pch.h"
#include "SceneRenderer.h"
#include "Variables.h"
#include "FrameGraph/GraphViz.h"

SceneRenderer::SceneRenderer()
{
	shadow_renderer.debug_renderer = &debug_renderer;
	shadow_renderer.entity_renderer = &entity_renderer;

	gbuffer_pass.entity_renderer = &entity_renderer;

	if (engine_ray_tracing)
	{
		///ray_tracing_scene = new RayTracingScene(nullptr);
		//shadow_renderer.ray_tracing_scene = ray_tracing_scene;
	}
}

void SceneRenderer::render(Camera *camera, RHITextureRef result_texture)
{
	PROFILE_CPU_FUNCTION();
	PROFILE_GPU_FUNCTION(gDynamicRHI->getCmdList());

	Renderer::setViewportSize(result_texture->getSize());
	Renderer::setCamera(camera);

	{
		PROFILE_CPU_SCOPE("SceneRenderer create render batches");
		render_batches.clear();

		BoundFrustum bound_frustum(camera->getProj(), camera->getView());
		auto entities_id = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
		for (entt::entity entity_id : entities_id)
		{
			Entity entity(entity_id);
			MeshRendererComponent &mesh_renderer = entity.getComponent<MeshRendererComponent>();

			for (int i = 0; i < mesh_renderer.meshes.size(); i++)
			{
				Engine::Mesh *mesh = mesh_renderer.meshes[i].getMesh();
				if (mesh == nullptr)
					continue;
				Material *material = mesh_renderer.materials.size() > i ? mesh_renderer.materials[i] : new Material();

				RenderBatch &batch = render_batches.emplace_back();
				batch.mesh = mesh;
				batch.material = material;
				batch.world_transform = entity.getWorldTransformMatrix();
				auto bbox = mesh->bound_box * batch.world_transform;
				batch.camera_visible = bbox.isInside(bound_frustum);
			}
		}
	}

	auto uniforms = Renderer::getDefaultUniforms();
	gDynamicRHI->setConstantBufferDataPerFrame(32, &uniforms, sizeof(uniforms));

	FrameGraph frameGraph;

	DefaultResourcesData &default_data = frameGraph.getBlackboard().add<DefaultResourcesData>();
	default_data.final = importTexture(frameGraph, result_texture);

	/*
	if (engine_ray_tracing)
	ray_tracing_scene->update();
	*/
	bool is_sky_dirty = sky_renderer.isDirty();
	sky_renderer.addProceduralPasses(frameGraph);
	LutData &lut_data = frameGraph.getBlackboard().add<LutData>();
	lut_data.brdf_lut = importTexture(frameGraph, lut_renderer.brdf_lut_texture);

	if (render_first_frame)
	{
		// Render BRDF Lut
		lut_renderer.addPasses(frameGraph);
	}

	IBLData &ibl_data = frameGraph.getBlackboard().add<IBLData>();
	ibl_data.irradiance = importTexture(frameGraph, irradiance_renderer.irradiance_texture);
	ibl_data.prefilter = importTexture(frameGraph, prefilter_renderer.prefilter_texture);

	if (render_first_frame || is_sky_dirty)
	{
		// Render IBL irradiance
		irradiance_renderer.addPass(frameGraph);

		// Render IBL prefilter
		prefilter_renderer.addPass(frameGraph);
	}

	render_first_frame = false;

	gbuffer_pass.AddPass(frameGraph, render_batches);

	// Shadows
	{
		shadow_renderer.camera = camera;
		if (render_shadows)
			shadow_renderer.addShadowMapPasses(frameGraph, render_batches);

		if (render_ray_traced_shadows)
			shadow_renderer.addRayTracedShadowPasses(frameGraph);
	}

	defferred_lighting_renderer.renderLights(frameGraph);
	ssao_renderer.addPasses(frameGraph);
	if (render_ssr)
		ssr_renderer.addPasses(frameGraph);
	auto &composite_data = frameGraph.getBlackboard().add<CompositeData>();
	// Draw Sky on background
	sky_renderer.addCompositePasses(frameGraph);

	// Draw composite (discard sky pixels by depth)
	deffered_composite_renderer.addPasses(frameGraph);

	// Render post process
	post_renderer.addPasses(frameGraph);

	// Render debug textures
	if (render_debug_rendering)
	{
		debug_renderer.addPasses(frameGraph);
	}
	debug_renderer.renderLines(frameGraph);

	frameGraph.compile();

	auto current_cmd_list = gDynamicRHI->getCmdList();
	frameGraph.execute(current_cmd_list);

	if (gInput.isKeyDown(GLFW_KEY_T))
	{
		GraphViz viz;
		viz.show("graph.dot", frameGraph);
	}
}
