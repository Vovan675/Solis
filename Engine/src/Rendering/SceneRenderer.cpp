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

	frame_graph.importTexture(GFXRID(FinalTexture), result_texture);

	frame_graph.importTexture(GFXRID(LutBRDF), lut_renderer.brdf_lut_texture);

	if (render_first_frame)
	{
		// Render BRDF Lut
		lut_renderer.addPasses(frame_graph);
	}

	bool is_sky_dirty = sky_renderer.isDirty();
	sky_renderer.addProceduralPasses(frame_graph);

	frame_graph.importTexture(GFXRID(IBLIrradiance), irradiance_renderer.irradiance_texture);
	frame_graph.importTexture(GFXRID(IBLPrefilter), prefilter_renderer.prefilter_texture);

	if (render_first_frame || is_sky_dirty)
	{
		uint32_t irradiance_samples = sky_renderer.getMode() == SKY_MODE_PROCEDURAL ? 64 : 4096;
		uint32_t prefilter_samples = sky_renderer.getMode() == SKY_MODE_PROCEDURAL ? 64 : 4096;
		irradiance_renderer.addPass(frame_graph, irradiance_samples);
		prefilter_renderer.addPass(frame_graph, prefilter_samples);
	}

	render_first_frame = false;

	gbuffer_pass.AddPass(frame_graph, render_batches);
	
	if (render_ssao)
		ssao_renderer.addPasses(frame_graph);

	{
		// Shadows
		if (render_shadows)
			shadow_renderer.addShadowMapPasses(frame_graph, render_batches);

		if (render_ray_traced_shadows)
			shadow_renderer.addRayTracedShadowPasses(frame_graph, rt_scene);
	}

	{
		// Lighting
		defferred_lighting_renderer.renderLights(frame_graph);

		deffered_composite_renderer.addPasses(frame_graph);
	}

	{
		// Forward
		sky_renderer.addCompositePasses(frame_graph);
	}

	// Render post process
	{
		// Postprocessing
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

struct MaterialGPU
{
	glm::vec4 albedo = glm::vec4(0, 0, 0, 1);
	uint32_t albedo_tex_id = 0;
	uint32_t metalness_tex_id = 0;
	uint32_t roughness_tex_id = 0;
	uint32_t specular_tex_id = 0;
	// metalness, roughness, specular
	glm::vec4 shading = glm::vec4(0, 0, 0.5, 1);
	uint32_t normal_tex_id = 0;
};

struct InstanceGPU
{
	glm::mat4 world_transform;
	glm::mat4 iworld_transform;
	uint32_t mesh_id;
	uint32_t material_id = 0;
};


struct MeshGPU
{
	uint32_t vertex_buffer_id;
	uint32_t index_buffer_id;
	uint32_t vertex_stride;
	uint32_t positions_offset;
	uint32_t normals_offset;
	uint32_t tangents_offset;
	uint32_t uvs_offset;
	uint32_t colors_offset;
};

void SceneRenderer::update(Camera *camera)
{
	if (engine_ray_tracing)
		rt_scene->update();

	if (render_shadows)
		shadow_renderer.updateShadows(camera);

	{
		PROFILE_CPU_SCOPE("SceneRenderer create render batches");
		render_batches.clear();

		eastl::vector<MaterialGPU> materials_gpu;
		eastl::vector<InstanceGPU> instances_gpu;
		eastl::vector<MeshGPU> meshes_gpu;
		
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
				batch.instance_id = instances_gpu.size();
				batch.world_transform = transform.getWorldTransform();
				batch.iworld_transform = transform.getInverseWorldTransform();
				batch.world_bound_box = mesh->bound_box * batch.world_transform;
				batch.camera_visible = batch.world_bound_box.isInside(camera_bound_frustum);

				InstanceGPU &instance = instances_gpu.emplace_back();
				instance.world_transform = transform.getWorldTransform();
				instance.iworld_transform = transform.getInverseWorldTransform();
				instance.mesh_id = meshes_gpu.size();
				instance.material_id = materials_gpu.size();

				MaterialGPU &material_gpu = materials_gpu.emplace_back();
				material_gpu.albedo = material->albedo;
				material_gpu.albedo_tex_id = material->albedo_tex.bindless_id;
				material_gpu.metalness_tex_id = material->metalness_tex.bindless_id;
				material_gpu.roughness_tex_id = material->roughness_tex.bindless_id;
				material_gpu.specular_tex_id = material->specular_tex.bindless_id;
				material_gpu.shading = glm::vec4(material->metalness, material->roughness, material->specular, 1.0f);
				material_gpu.normal_tex_id = material->normal_tex.bindless_id;

				MeshGPU &mesh_gpu = meshes_gpu.emplace_back();
				mesh_gpu.vertex_buffer_id = gDynamicRHI->getBindlessResources()->addBuffer(mesh->vertexBuffer);
				mesh_gpu.index_buffer_id = gDynamicRHI->getBindlessResources()->addBuffer(mesh->indexBuffer);
				mesh_gpu.vertex_stride = sizeof(Engine::Vertex);
				mesh_gpu.positions_offset = offsetof(Engine::Vertex, pos);
				mesh_gpu.normals_offset = offsetof(Engine::Vertex, normal);
				mesh_gpu.tangents_offset = offsetof(Engine::Vertex, tangent);
				mesh_gpu.uvs_offset = offsetof(Engine::Vertex, uv);
				mesh_gpu.colors_offset = offsetof(Engine::Vertex, color);
			}
		}

		auto fill_buffer = [](RHIBufferRef &buffer, void *data, size_t count, size_t stride, const char *name)
		{
			uint32_t buffer_size = count * stride;
			if (!buffer || buffer->getSize() < buffer_size)
			{
				BufferDescription desc;
				desc.size = buffer_size;
				desc.usage = STORAGE_BUFFER;
				desc.useStagingBuffer = false;
				desc.stride = stride;
				buffer = gDynamicRHI->createBuffer(desc);
				buffer->setDebugName(name);
			}
			buffer->fill(data);
		};

		fill_buffer(materials_buffer, materials_gpu.data(), materials_gpu.size(), sizeof(MaterialGPU), "Materials Buffer");
		fill_buffer(instances_buffer, instances_gpu.data(), instances_gpu.size(), sizeof(InstanceGPU), "Instances Buffer");
		fill_buffer(meshes_buffer, meshes_gpu.data(), meshes_gpu.size(), sizeof(MeshGPU), "Meshes Buffer");
	}

	auto uniforms = Renderer::getDefaultUniforms();
	uniforms.materials_buffer_id = gDynamicRHI->getBindlessResources()->addBuffer(materials_buffer);
	uniforms.instances_buffer_id = gDynamicRHI->getBindlessResources()->addBuffer(instances_buffer);
	uniforms.meshes_buffer_id = gDynamicRHI->getBindlessResources()->addBuffer(meshes_buffer);
	gDynamicRHI->setConstantBufferDataPerFrame(32, &uniforms, sizeof(uniforms));
}
