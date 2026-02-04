#include "pch.h"
#include "SceneRenderer.h"
#include "Core/Variables.h"
#include "FrameGraph/GraphViz.h"
#include "Math/BoundSphere.h"
#include "GlobalBufferCache.h"

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

	if (engine_ray_tracing && render_path_tracing)
	{
		render_path_traced(camera, frame_graph);
	} else
	{
		render_deferred(camera, frame_graph);
	}

	frame_graph.compile();
	frame_graph.execute(gDynamicRHI->getCmdList());

	if (gInput.isKeyDown(GLFW_KEY_G))
	{
		GraphViz viz;
		viz.show("graph.dot", frame_graph);
	}
	render_first_frame = false;
}

void SceneRenderer::render_deferred(Camera *camera, FrameGraph &frame_graph)
{
	gpu_frame_cull(frame_graph);

	gbuffer_pass.addPass(frame_graph, indirect_draw_calls_max_count);

	if (render_ssao)
		ssao_renderer.addPasses(frame_graph);

	if (render_ddgi)
		ddgi_renderer.addPasses(frame_graph, rt_scene);

	{
		// Shadows
		if (render_shadows)
		{
			if (render_ray_traced_shadows && engine_ray_tracing)
				shadow_renderer.addRayTracedShadowPasses(frame_graph, rt_scene);
			else
				shadow_renderer.addShadowMapPasses(frame_graph, indirect_draw_calls_max_count);
		}
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

	if (render_ddgi && render_ddgi_visualize)
		ddgi_renderer.addVisualizePass(frame_graph);

	// Render post process
	{
		// Postprocessing
		if (render_ssr)
			ssr_renderer.addPasses(frame_graph);

		post_renderer.addPasses(frame_graph);
	}

	debug_renderer.addPasses(frame_graph);
}

void SceneRenderer::render_path_traced(Camera *camera, FrameGraph &frame_graph)
{
	path_tracing_renderer.addPass(frame_graph, rt_scene);
	post_renderer.addPasses(frame_graph);
}

void SceneRenderer::update(Camera *camera)
{
	main_view_camera = camera;

	if (engine_ray_tracing)
		rt_scene->update();

	if (render_shadows)
		shadow_renderer.updateShadows(camera);

	{
		PROFILE_CPU_SCOPE("SceneRenderer create render batches");

		frustums.clear();
		materials.clear();
		meshes.clear();
		instances.clear();
		instances_pass_masks.clear();

		FrustumDataGPU frustum_data;
		frustum_data.view_projection = camera->getProj() * camera->getView();
		frustum_data.pass_mask = PASS_MASK_GBUFFER;
		frustums.push_back(frustum_data);

		auto light_entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
		for (entt::entity light_entity_id : light_entities_id)
		{
			Entity light_entity(light_entity_id);
			auto &light = light_entity.getComponent<LightComponent>();

			glm::vec3 scale, position, skew;
			glm::vec4 persp;
			glm::quat rotation;
			glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);


			if (light.getType() == LIGHT_TYPE_POINT)
			{
				FrustumDataGPU frustum_data;
				frustum_data.pass_mask = PASS_MASK_POINT_SHADOW;

				eastl::vector<glm::mat4> faces_transforms;
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, 1)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)));

				for (int face = 0; face < 6; face++)
				{
					frustum_data.view_projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, 0.05f, light.radius) * faces_transforms[face];
					frustums.push_back(frustum_data);
				}
			} else if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
			{
				FrustumDataGPU frustum_data;
				frustum_data.pass_mask = PASS_MASK_DIRECTIONAL_SHADOW;
				frustum_data.is_ortho = true;

				for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
				{
					frustum_data.view_projection = light.getCascadeViewProj(i);
					frustums.push_back(frustum_data);
				}
			}
		}


		auto entities_view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();

		// 1. Collect all objects that will render and their batch keys
		struct RenderObject
		{
			size_t batch_key;
			TransformComponent *transform;
			Engine::Mesh *mesh;
			Material *material;

			bool operator <(const RenderObject &other) const
			{
				return other.batch_key < batch_key;
			}
		};

		eastl::vector<RenderObject> render_objects;

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

				RenderObject &render_object = render_objects.emplace_back();

				render_object.batch_key = mesh->id; // per mesh batching
				render_object.transform = &transform;
				render_object.mesh = mesh;
				render_object.material = material;
			}
		}

		// 2. Sort objects by batch keys
		//eastl::quick_sort(render_objects.begin(), render_objects.end());

		uint32_t draw_calls_count = 0;
		// 3. Create render batches based on list for every view (TODO: move to GPU compute shader)

		// Big Groups fast culling
		for (int i = 0; i < render_objects.size(); i++)
		{
			RenderObject &render_object = render_objects[i];
			TransformComponent *transform = render_object.transform;
			Engine::Mesh *mesh = render_object.mesh;
			Material *material = render_object.material;

			// CPU Part (collect all instances and their bboxes that can be drawn)
			// TODO: coarse culling based on tree hierarchy needed here for big worlds

			InstanceGPU &instance = instances.emplace_back();
			instance.world_transform = transform->getWorldTransform();
			instance.iworld_transform = transform->getInverseWorldTransform();
			instance.mesh_id = meshes.size();
			instance.material_id = materials.size();

			BoundBox bound_box(mesh->bound_box);

			//BoundSphere bound_sphere(bound_box);
			//instance.bound_sphere = glm::vec4(bound_sphere.center, bound_sphere.radius);

			instance.bound_center = glm::vec4(bound_box.getCenter(), 1.0f);
			instance.bound_extent = glm::vec4(bound_box.getSize() / 2.0f, 1.0);


			MaterialGPU &material_gpu = materials.emplace_back();
			material_gpu.albedo = material->albedo;
			material_gpu.albedo_tex_id = material->albedo_tex.bindless_id;
			material_gpu.metalness_tex_id = material->metalness_tex.bindless_id;
			material_gpu.roughness_tex_id = material->roughness_tex.bindless_id;
			material_gpu.specular_tex_id = material->specular_tex.bindless_id;
			material_gpu.shading = glm::vec4(material->metalness, material->roughness, material->specular, 1.0f);
			material_gpu.normal_tex_id = material->normal_tex.bindless_id;

			MeshGPU &mesh_gpu = meshes.emplace_back();
			mesh_gpu.vertex_buffer_id = GlobalBufferCache::getGlobalVertexBuffer()->getShaderResourceView()->getBindlessIndex();
			mesh_gpu.index_buffer_id = mesh->indexBuffer->getShaderResourceView()->getBindlessIndex();
			mesh_gpu.vertex_stride = sizeof(Engine::Vertex);

			uint32_t vertex_offset = mesh->global_vertex_buffer_offset * sizeof(Engine::Vertex);
			mesh_gpu.positions_offset = vertex_offset + offsetof(Engine::Vertex, pos);
			mesh_gpu.normals_offset = vertex_offset + offsetof(Engine::Vertex, normal);
			mesh_gpu.tangents_offset = vertex_offset + offsetof(Engine::Vertex, tangent);
			mesh_gpu.uvs_offset = vertex_offset + offsetof(Engine::Vertex, uv);
			mesh_gpu.colors_offset = vertex_offset + offsetof(Engine::Vertex, color);
			mesh_gpu.colors_offset = mesh->global_vertex_buffer_offset;
			mesh_gpu.indices_count = mesh->indices.size();
			mesh_gpu.index_offset = mesh->global_index_buffer_offset;
		}

		indirect_draw_calls_max_count = instances.size();

		auto fill_buffer = [](RHIBufferRef &buffer, void *data, size_t count, size_t stride, const char *name, BufferUsage usage = BufferUsage::SHADER_READ_BUFFER, bool use_staging = false)
		{
			uint32_t buffer_size = count * stride;
			if (!buffer || buffer->getSize() < buffer_size)
			{
				BufferDescription desc;
				desc.size = buffer_size;
				desc.usage = usage;
				desc.use_staging_buffer = use_staging;
				desc.storage_stride = stride;
				buffer = gDynamicRHI->createBuffer(desc);
				buffer->setDebugName(name);
			}

			if (data)
				buffer->fill(data);
		};


		fill_buffer(frustums_gpu, frustums.data(), frustums.size(), sizeof(FrustumDataGPU), "Frustums Buffer");

		fill_buffer(materials_gpu, materials.data(), materials.size(), sizeof(MaterialGPU), "Materials Buffer");
		fill_buffer(instances_gpu, instances.data(), instances.size(), sizeof(InstanceGPU), "Instances Buffer");
		fill_buffer(meshes_gpu, meshes.data(), meshes.size(), sizeof(MeshGPU), "Meshes Buffer");
	}

	auto uniforms = Renderer::getDefaultUniforms();
	uniforms.materials_buffer_id = materials_gpu->getShaderResourceView()->getBindlessIndex();
	uniforms.instances_buffer_id = instances_gpu->getShaderResourceView()->getBindlessIndex();
	uniforms.meshes_buffer_id = meshes_gpu->getShaderResourceView()->getBindlessIndex();
	uniforms.tlas_id = engine_ray_tracing ? rt_scene->getTopLevelAS()->getBindlessId() : 0;
	uniforms.ddgi_volume_buffer_id = render_ddgi ? ddgi_renderer.getVolumeBufferId() : 0;
	uniforms.lines_gpu_buffer_id = debug_renderer.getLinesGpuBuffer()->getUnorderedAccessView()->getBindlessIndex();
	gDynamicRHI->setConstantBufferDataPerFrame(32, &uniforms, sizeof(uniforms));
}

void SceneRenderer::gpu_frame_cull(FrameGraph &frame_graph)
{
	// Cull Instances
	frame_graph.addCallbackPass("GPU Frame Cull Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createBuffer(GFXRID(InstancesPassMask), sizeof(uint32_t), indirect_draw_calls_max_count, BufferUsage::SHADER_WRITE_BUFFER);
		builder.writeBuffer(GFXRID(InstancesPassMask));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		if (render_freeze_culling)
			return;

		frustums_gpu->transitState(ResourceState::SHADER_RESOURCE);

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/frame_culling.hlsl", COMPUTE_SHADER, "CS_CullInstances"));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t frustums_buffer_id;
			uint32_t frustums_count;
			uint32_t instances_count;
			uint32_t instances_pass_mask_buffer_id;
		} constants;

		constants.frustums_buffer_id = frustums_gpu->getShaderResourceView()->getBindlessIndex();
		constants.frustums_count = frustums.size();
		constants.instances_count = instances.size();
		constants.instances_pass_mask_buffer_id = resources.getReadWriteBuffer(GFXRID(InstancesPassMask));

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		int num_groups = ceil(constants.instances_count / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
	});
}
