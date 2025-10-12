#include "pch.h"
#include "SceneRenderer.h"
#include "Core/Variables.h"
#include "FrameGraph/GraphViz.h"

SceneRenderer::SceneRenderer()
{
	shadow_renderer.debug_renderer = &debug_renderer;

	gbuffer_pass.entity_renderer = &entity_renderer;
}

void SceneRenderer::setScene(Ref<Scene> scene)
{
	if (this->scene == scene)
		return;

	this->scene = scene;
	if (engine_ray_tracing)
	{
		rt_scene = new RayTracingScene(scene);
	} else
	{
		rt_scene = nullptr;
	}
}

void SceneRenderer::render(Camera *camera, RHITextureRef result_texture)
{
	PROFILE_CPU_FUNCTION();
	PROFILE_GPU_FUNCTION(gDynamicRHI->getCmdList());

	Renderer::setViewportSize(result_texture->getSize());
	Renderer::setCamera(camera);

	update(camera);

	FrameGraph frame_graph;

	DefaultResourcesData &default_data = frame_graph.getBlackboard().add<DefaultResourcesData>();
	default_data.final = importTexture(frame_graph, result_texture);

	LutData &lut_data = frame_graph.getBlackboard().add<LutData>();
	lut_data.brdf_lut = importTexture(frame_graph, lut_renderer.brdf_lut_texture);

	if (render_first_frame)
	{
		// Render BRDF Lut
		lut_renderer.addPasses(frame_graph);
	}

	bool is_sky_dirty = sky_renderer.isDirty();
	sky_renderer.addProceduralPasses(frame_graph);

	IBLData &ibl_data = frame_graph.getBlackboard().add<IBLData>();
	ibl_data.irradiance = importTexture(frame_graph, irradiance_renderer.irradiance_texture);
	ibl_data.prefilter = importTexture(frame_graph, prefilter_renderer.prefilter_texture);

	if (render_first_frame || is_sky_dirty)
	{
		uint32_t irradiance_samples = sky_renderer.getMode() == SKY_MODE_PROCEDURAL ? 64 : 4096;
		uint32_t prefilter_samples = sky_renderer.getMode() == SKY_MODE_PROCEDURAL ? 64 : 4096;
		irradiance_renderer.addPass(frame_graph, irradiance_samples);
		prefilter_renderer.addPass(frame_graph, prefilter_samples);
	}

	render_first_frame = false;

	gbuffer_pass.AddPass(frame_graph, render_batches);
	ssao_renderer.addPasses(frame_graph);

	{
		PROFILE_GPU_SCOPE_VAR(gDynamicRHI->getCmdList(), "Shadows")
		if (render_shadows)
			shadow_renderer.addShadowMapPasses(frame_graph, render_batches);

		if (render_ray_traced_shadows)
			shadow_renderer.addRayTracedShadowPasses(frame_graph, rt_scene);
	}

	{
		PROFILE_GPU_SCOPE_VAR(gDynamicRHI->getCmdList(), "Lighting")
		defferred_lighting_renderer.renderLights(frame_graph);

		auto &composite_data = frame_graph.getBlackboard().add<CompositeData>();
		deffered_composite_renderer.addPasses(frame_graph);
	}

	{
		PROFILE_GPU_SCOPE_VAR(gDynamicRHI->getCmdList(), "Forward")
		sky_renderer.addCompositePasses(frame_graph);
	}

	// Render post process
	{
		PROFILE_GPU_SCOPE_VAR(gDynamicRHI->getCmdList(), "Postprocessing")
		if (render_ssr)
			ssr_renderer.addPasses(frame_graph);

		post_renderer.addPasses(frame_graph);
	}

	debug_renderer.addPasses(frame_graph);

	frame_graph.compile();
	frame_graph.execute(gDynamicRHI->getCmdList());

	if (gInput.isKeyDown(GLFW_KEY_G))
	{
		GraphViz viz;
		viz.show("graph.dot", frame_graph);
	}
}

void SceneRenderer::update(Camera *camera)
{
	auto uniforms = Renderer::getDefaultUniforms();
	gDynamicRHI->setConstantBufferDataPerFrame(32, &uniforms, sizeof(uniforms));

	if (engine_ray_tracing)
		rt_scene->update();

	if (render_shadows)
		shadow_renderer.updateShadows(camera);

	{
		PROFILE_CPU_SCOPE("SceneRenderer create render batches");
		render_batches.clear();

		BoundFrustum camera_bound_frustum(camera->getProj(), camera->getView());
		auto entities_view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
		for (entt::entity entity_id : entities_view)
		{
			TransformComponent &transform = entities_view.get<TransformComponent>(entity_id);
			MeshRendererComponent &mesh_renderer = entities_view.get<MeshRendererComponent>(entity_id);

			for (int i = 0; i < mesh_renderer.meshes.size(); i++)
			{
				Engine::Mesh *mesh = mesh_renderer.meshes[i].getMesh();
				if (mesh == nullptr)
					continue;
				Material *material = mesh_renderer.materials.size() > i ? mesh_renderer.materials[i] : new Material();
				material->update();

				RenderBatch &batch = render_batches.emplace_back();
				batch.mesh = mesh;
				batch.material = material;
				batch.world_transform = transform.getWorldTransform();
				batch.iworld_transform = transform.getInverseWorldTransform();
				batch.world_bound_box = mesh->bound_box * batch.world_transform;
				batch.camera_visible = batch.world_bound_box.isInside(camera_bound_frustum);
			}
		}
	}
}
