#include "pch.h"
#include "SceneRenderer.h"
#include "Core/Variables.h"
#include "FrameGraph/GraphViz.h"
#include "Math/BoundSphere.h"
#include "GlobalBufferCache.h"
#include "Rendering/Model.h"
#include "Rendering/UploadManager.h"
#include "Assets/AssetManager.h"

SceneRenderer::SceneRenderer()
{
	shadow_renderer.debug_renderer = &debug_renderer;
	geometry_streaming.init();

	frustums_table.init("Frustums Buffer", 64, ReplicationPolicy::Copy);
	materials_table.init("Materials Buffer", 512, ReplicationPolicy::DirtyRows);
	meshes_table.init("Meshes Buffer", 4096, ReplicationPolicy::DirtyRows);
	instances_table.init("Instances Buffer", 65536, ReplicationPolicy::DirtyRows);

	AssetManager::onPreReimport().connect<&SceneRenderer::on_asset_pre_reimport>(this);
	AssetManager::onPostReimport().connect<&SceneRenderer::on_asset_post_reimport>(this);
}

SceneRenderer::~SceneRenderer()
{
	AssetManager::onPreReimport().disconnect(this);
	AssetManager::onPostReimport().disconnect(this);
}

void SceneRenderer::setScene(Ref<Scene> scene)
{
	if (this->scene == scene)
		return;

	if (this->scene)
	{
		this->scene->registry.on_construct<MeshRendererComponent>().disconnect<&SceneRenderer::on_mesh_renderer_constructed>(this);
		this->scene->registry.on_destroy<MeshRendererComponent>().disconnect<&SceneRenderer::on_mesh_renderer_destroyed>(this);
	}

	this->scene = scene;
	if (engine_ray_tracing)
	{
		rt_scene = new RayTracingScene();
	} else
	{
		rt_scene = nullptr;
	}

	entity_instances.clear();
	instances_table.reset();
	materials_table.reset();
	meshes_table.reset();

	if (!scene)
		return;

	scene->registry.on_construct<MeshRendererComponent>().connect<&SceneRenderer::on_mesh_renderer_constructed>(this);
	scene->registry.on_destroy<MeshRendererComponent>().connect<&SceneRenderer::on_mesh_renderer_destroyed>(this);

	for (entt::entity entity : scene->registry.view<MeshRendererComponent>())
		scene->markDirty(entity, DIRTY_RENDER_STATE);
}

void SceneRenderer::on_mesh_renderer_constructed(entt::registry &registry, entt::entity entity)
{
	scene->markDirty(entity, DIRTY_RENDER_STATE);
}

void SceneRenderer::on_mesh_renderer_destroyed(entt::registry &registry, entt::entity entity)
{
	free_instances(entity);
}

void SceneRenderer::free_instances(entt::entity entity_id)
{
	auto it = entity_instances.find(entity_id);
	if (it == entity_instances.end())
		return;

	InstanceGPU empty_instance{};
	empty_instance.flags = INSTANCE_FLAG_INVALID;
	for (uint32_t i = 0; i < it->second.count; i++)
	{
		instances_table.set(it->second.start + i, empty_instance);
		if (rt_scene)
			rt_scene->removeInstance(it->second.start + i);
	}
	instances_table.freeArray(it->second.start, it->second.count);
	entity_instances.erase(it);
}

void SceneRenderer::refresh_meshes(entt::entity entity_id, MeshRendererComponent &mesh_renderer)
{
	auto it = entity_instances.find(entity_id);
	if (it == entity_instances.end())
		return;

	for (int i = 0; i < it->second.count && i < mesh_renderer.meshes.size(); i++)
	{
		Engine::Mesh *mesh = mesh_renderer.meshes[i].getMesh();
		if (!mesh)
			continue;

		const Engine::MeshletFileView *file_view = mesh_renderer.meshes[i].getModel()->getFileView(mesh->id);
		if (file_view)
			geometry_streaming.registerMesh(mesh, *file_view);

		MeshGPU mesh_gpu{};
		mesh_gpu.vertex_buffer_id = mesh->indexed && mesh->indexed->vertex_buffer ? mesh->indexed->vertex_buffer->getShaderResourceView()->getBindlessIndex() : 0;
		mesh_gpu.index_buffer_id = mesh->indexed && mesh->indexed->index_buffer ? mesh->indexed->index_buffer->getShaderResourceView()->getBindlessIndex() : 0;
		mesh_gpu.vertex_stride = sizeof(Engine::Vertex);
		mesh_gpu.positions_offset = offsetof(Engine::Vertex, pos);
		mesh_gpu.normals_offset = offsetof(Engine::Vertex, normal);
		mesh_gpu.tangents_offset = offsetof(Engine::Vertex, tangent);
		mesh_gpu.uvs_offset = offsetof(Engine::Vertex, uv);
		mesh_gpu.indices_count = mesh->indexed ? mesh->indexed->indices.size() : 0;
		if (mesh->useMeshlets())
		{
			GlobalBufferCache::MeshGlobalOffsets mesh_global = GlobalBufferCache::getMeshOffsets(mesh->id);
			mesh_gpu.meshlet_lod_groups_offset = mesh_global.lod_groups_offset;
			mesh_gpu.group_residency_offset = geometry_streaming.getMeshResidencyOffset(mesh);
			mesh_gpu.lod_nodes_offset = mesh_global.lod_nodes_offset;
		}
		mesh_gpu.root_group_offset = mesh->meshlet_data ? mesh->meshlet_data->meshlet_root_group_local_offset : 0;
		mesh_gpu.attribute_flags = mesh->attribute_flags;
		mesh_gpu.flags = mesh->useMeshlets() ? MESH_FLAG_MESHLET : 0;

		meshes_table.set(it->second.start + i, mesh_gpu); // In future every mesh would hold its own slot and dont hold duplicates
	}
}

void SceneRenderer::refresh_materials(entt::entity entity_id, MeshRendererComponent &mesh_renderer)
{
	auto it = entity_instances.find(entity_id);
	if (it == entity_instances.end())
		return;

	for (int i = 0; i < it->second.count && i < mesh_renderer.meshes.size(); i++)
	{
		Material *material = mesh_renderer.getMaterial(i);
		if (!material)
			continue;

		material->update();

		MaterialGPU material_gpu{};
		if (render_lighting_only)
		{
			material_gpu.albedo = glm::vec4(glm::vec3(LightingOnlyMaterial::albedo), 1.0f);
			material_gpu.shading = glm::vec4(LightingOnlyMaterial::metalness, LightingOnlyMaterial::roughness, LightingOnlyMaterial::specular, 1.0f);
		} else
		{
			material_gpu.albedo = material->albedo;
			material_gpu.shading = glm::vec4(material->metalness, material->roughness, material->specular, 1.0f);
			material_gpu.albedo_tex_id = material->albedo_tex.bindless_id;
			material_gpu.metalness_tex_id = material->metalness_tex.bindless_id;
			material_gpu.roughness_tex_id = material->roughness_tex.bindless_id;
			material_gpu.specular_tex_id = material->specular_tex.bindless_id;
			material_gpu.normal_tex_id = material->normal_tex.bindless_id;
		}

		materials_table.set(it->second.start + i, material_gpu); // In future materials would be globally unique, so every material would hold its own slot
	}
}

void SceneRenderer::refresh_transforms(entt::entity entity_id)
{
	auto it = entity_instances.find(entity_id);
	if (it == entity_instances.end())
		return;

	Entity entity(entity_id);
	const TransformComponent &transform = entity.getComponent<TransformComponent>();
	MeshRendererComponent &mesh_renderer = entity.getComponent<MeshRendererComponent>();

	for (int i = 0; i < it->second.count && i < mesh_renderer.meshes.size(); i++)
	{
		uint32_t slot = it->second.start + i;
		Engine::Mesh *mesh = mesh_renderer.meshes[i].getMesh();

		// If no mesh, then instance is invalid (skip it)
		if (!mesh)
		{
			InstanceGPU empty_instance{};
			empty_instance.flags = INSTANCE_FLAG_INVALID;
			instances_table.set(slot, empty_instance);
			continue;
		}

		InstanceGPU instance{};
		instance.world_transform = transform.getWorldTransform();
		instance.iworld_transform = transform.getInverseWorldTransform();
		instance.old_world_transform = transform.getOldWorldTransform();
		instance.mesh_id = slot;
		instance.material_id = slot;
		BoundBox bound_box(mesh->bound_box);
		instance.bound_center = glm::vec4(bound_box.getCenter(), 1.0f);
		instance.bound_extent = glm::vec4(bound_box.getSize() / 2.0f, 1.0);

		instances_table.set(slot, instance);
		if (rt_scene)
			rt_scene->setInstance(slot, mesh, transform.getWorldTransform());
	}
}

void SceneRenderer::on_asset_pre_reimport(Asset *asset)
{
	if (!scene || asset->type != AssetManager::getTypeInfo<Model>())
		return;

	Model *model = static_cast<Model *>(asset);
	eastl::vector<Ref<Engine::Mesh>> old_meshes;
	model->getMeshes(old_meshes);
	for (auto &old_mesh : old_meshes)
	{
		geometry_streaming.unregisterMesh(old_mesh);
		if (rt_scene)
			rt_scene->invalidateMesh(old_mesh);
	}
}

void SceneRenderer::on_asset_post_reimport(Asset *asset)
{
	if (!scene)
		return;

	if (asset->type == AssetManager::getTypeInfo<RHITexture>())
	{
		auto view = scene->registry.view<MeshRendererComponent>();
		for (entt::entity entity : view)
		{
			MeshRendererComponent &mesh_renderer = view.get<MeshRendererComponent>(entity);
			for (int i = 0; i < mesh_renderer.meshes.size(); i++)
			{
				Material *material = mesh_renderer.getMaterial(i);
				if (!material || !material->usesTexture(asset->guid))
					continue;

				material->invalidateTextures();
				scene->markDirty(entity, DIRTY_MATERIAL);
			}
		}
	} else if (asset->type == AssetManager::getTypeInfo<Material>())
	{
		auto view = scene->registry.view<MeshRendererComponent>();
		for (entt::entity entity : view)
		{
			MeshRendererComponent &mesh_renderer = view.get<MeshRendererComponent>(entity);
			for (int i = 0; i < mesh_renderer.meshes.size(); i++)
			{
				if (mesh_renderer.getMaterial(i) != asset)
					continue;

				scene->markDirty(entity, DIRTY_MATERIAL);
				break;
			}
		}
	} else if (asset->type == AssetManager::getTypeInfo<Model>())
	{
		Model *model = static_cast<Model *>(asset);
		auto view = scene->registry.view<MeshRendererComponent>();
		for (entt::entity entity : view)
		{
			MeshRendererComponent &mesh_renderer = view.get<MeshRendererComponent>(entity);
			for (auto &mesh_id : mesh_renderer.meshes)
			{
				if (mesh_id.getModel() == model)
				{
					scene->markDirty(entity, DIRTY_RENDER_STATE);
					break;
				}
			}
		}
	}
}

void SceneRenderer::render(Camera *camera, RHITextureRef result_texture)
{
	PROFILE_CPU_FUNCTION();
	PROFILE_GPU_FUNCTION(gDynamicRHI->getCmdList());

	glm::ivec2 output_resolution = result_texture->getSize();
	glm::ivec2 render_resolution = upscale_renderer.getRenderResolution(output_resolution);

	Renderer::setOutputResolution(output_resolution);
	Renderer::setRenderResolution(render_resolution);
	Renderer::setCamera(camera);

	gUploadManager->beginFrame();

	update(camera);

	FrameGraph frame_graph;

	frame_graph.importTexture(GFXRID(FinalTexture), result_texture);
	frame_graph.importTexture(GFXRID(LutBRDF), lut_renderer.brdf_lut_texture);
	geometry_streaming.importBuffers(frame_graph);

	frustums_table.upload(frame_graph);
	instances_table.upload(frame_graph);
	materials_table.upload(frame_graph);
	meshes_table.upload(frame_graph);

	if (render_first_frame)
	{
		// Render BRDF Lut
		lut_renderer.addPasses(frame_graph);
	}

	sky_renderer.addProceduralPasses(frame_graph);

	frame_graph.importTexture(GFXRID(IBLIrradiance), irradiance_renderer.irradiance_texture);
	frame_graph.importTexture(GFXRID(IBLPrefilter), prefilter_renderer.prefilter_texture);

	if (sky_renderer.isDirty())
	{
		uint32_t irradiance_samples = GFXOPTIONS(sky).mode == SKY_MODE_PROCEDURAL ? 64 : 4096;
		uint32_t prefilter_samples = GFXOPTIONS(sky).mode == SKY_MODE_PROCEDURAL ? 64 : 4096;
		irradiance_renderer.addPass(frame_graph, irradiance_samples);
		prefilter_renderer.addPass(frame_graph, prefilter_samples);
	}

	if (engine_ray_tracing && GFXOPTIONS(path_tracing))
	{
		render_path_traced(camera, frame_graph);
	} else
	{
		render_deferred(camera, frame_graph);
	}

	gUploadManager->flush(frame_graph);

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

	if (GFXOPTIONS(ssao).enabled)
		ssao_renderer.addPasses(frame_graph);

	if (GFXOPTIONS(ddgi).enabled)
		ddgi_renderer.addPasses(frame_graph, rt_scene);

	{
		// Shadows
		if (GFXOPTIONS(shadows).enabled)
		{
			if (Renderer::isRayTracedShadowsEnabled())
				shadow_renderer.addRayTracedShadowPasses(frame_graph, rt_scene);
			shadow_renderer.addShadowMapPasses(frame_graph, indirect_draw_calls_max_count);
		}
	}

	geometry_streaming.addAgeFilterAndReadbackPasses(frame_graph);

	{
		// Lighting
		defferred_lighting_renderer.renderLights(frame_graph);

		deffered_composite_renderer.addPasses(frame_graph);
	}

	{
		// Forward
		if (GFXOPTIONS(sky).enabled)
			sky_renderer.addCompositePasses(frame_graph);
	}

	if (GFXOPTIONS(ddgi).enabled && render_ddgi_visualize)
		ddgi_renderer.addVisualizePass(frame_graph);

	// Render post process (render resolution posts -> upscale -> outpu resolution posts)
	{
		if (GFXOPTIONS(ssr).enabled)
			ssr_renderer.addPasses(frame_graph);

		upscale_renderer.addPasses(frame_graph);

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

	if (GFXOPTIONS(shadows).enabled)
		shadow_renderer.updateShadows(camera);

	{
		PROFILE_CPU_SCOPE("SceneRenderer update render data");

		frustums.clear();

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
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, -1)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, 1)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)));
				faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)));

				for (int face = 0; face < 6; face++)
				{
					frustum_data.view_projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, 0.05f, light.attenuation_radius) * faces_transforms[face];
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

		frustums_table.setArray(0, eastl::span<const FrustumDataGPU>(frustums.data(), frustums.size()));

		geometry_streaming.update();

		static bool last_render_lighting_only = render_lighting_only;
		if (last_render_lighting_only != render_lighting_only)
		{
			last_render_lighting_only = render_lighting_only;
			for (entt::entity entity_id : scene->getEntitiesWith<MeshRendererComponent>())
				scene->markDirty(entity_id, DIRTY_MATERIAL);
			render_path_tracing_first_frame = true;
		}

		eastl::hash_set<entt::entity> moved_this_frame;

		for (entt::entity entity_id : scene->getDirtyList())
		{
			if (!scene->registry.valid(entity_id) || !scene->registry.all_of<MeshRendererComponent>(entity_id))
				continue;

			Entity entity(entity_id);
			MeshRendererComponent &mesh_renderer = entity.getComponent<MeshRendererComponent>();

			uint32_t flags = scene->getDirtyFlags(entity_id);
			if (flags & DIRTY_RENDER_STATE)
			{
				free_instances(entity_id);
				if (mesh_renderer.meshes.empty())
					continue;

				uint32_t count = mesh_renderer.meshes.size();
				entity_instances[entity_id] = { instances_table.allocate(count), count };

				refresh_meshes(entity_id, mesh_renderer);
				refresh_materials(entity_id, mesh_renderer);
				refresh_transforms(entity_id);
			} else if (flags & DIRTY_MATERIAL)
			{
				refresh_materials(entity_id, mesh_renderer);
			} else if (flags & DIRTY_TRANSFORM)
			{
				refresh_transforms(entity_id);
				moved_this_frame.insert(entity_id);
			}
		}

		// Reupload moved objects old transformation once more (so old_position would be the same as position)
		for (entt::entity entity_id : moved_last_frame_entities)
		{
			if (scene->getDirtyFlags(entity_id) & DIRTY_TRANSFORM)
				continue;
			refresh_transforms(entity_id);
		}
		moved_last_frame_entities = moved_this_frame;

		indirect_draw_calls_max_count = instances_table.getMaxUsedSlot();
		scene->clearDirty();

		if (engine_ray_tracing && rt_scene)
			rt_scene->update(camera);

		if (render_meshlets_bvh_visualize)
		{
			auto depthColor = [](int depth) -> glm::vec3
			{
				static const glm::vec3 palette[] = {
					{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f},
					{1.f, 1.f, 0.f}, {1.f, 0.f, 1.f}, {0.f, 1.f, 1.f},
				};
				return palette[depth % (sizeof(palette) / sizeof(palette[0]))];
			};

			int target_depth = render_meshlets_bvh_visualize_depth;

			auto view = scene->getEntitiesWith<TransformComponent, MeshRendererComponent>();
			for (entt::entity entity_id : view)
			{
				auto &transform = view.get<TransformComponent>(entity_id);
				auto &mesh_renderer = view.get<MeshRendererComponent>(entity_id);
				for (int i = 0; i < mesh_renderer.meshes.size(); i++)
				{
					const Engine::Mesh *mesh = mesh_renderer.meshes[i].getMesh();
					if (!mesh || !mesh->meshlet_data)
						continue;

					glm::mat4 world = transform.getWorldTransform();
					float uniform_scale = glm::length(glm::vec3(world[0]));

					struct VisItem { uint32_t node_idx; int depth; };
					eastl::queue<VisItem> q;
					q.push({mesh->meshlet_data->meshlet_root_group_local_offset, 0});

					while (!q.empty())
					{
						auto [idx, depth] = q.front();
						q.pop();
						const LodNode &node = mesh->meshlet_data->lod_nodes[idx];

						bool is_leaf = (node.child_count == 0);
						bool draw = (target_depth < 0) ? !is_leaf : (depth == target_depth);

						if (draw)
						{
							glm::vec3 wc = glm::vec3(world * glm::vec4(node.center, 1.f));
							debug_renderer.addSphere(wc, node.radius * uniform_scale, 16, depthColor(depth));
						}

						if (!is_leaf && (target_depth < 0 || depth < target_depth))
						{
							for (uint32_t c = 0; c < node.child_count; c++)
								q.push({node.first_child + c, depth + 1});
						}
					}

				}
			}
		}
	}

	auto uniforms = Renderer::getDefaultUniforms();
	uniforms.sky_intensity = GFXOPTIONS(sky).getIntensity();
	uniforms.camera_exposure = GFXOPTIONS(film).getExposure();
	uniforms.materials_buffer_id = materials_table.getBindlessIndex();
	uniforms.instances_buffer_id = instances_table.getBindlessIndex();
	uniforms.meshes_buffer_id = meshes_table.getBindlessIndex();
	uniforms.tlas_id = engine_ray_tracing ? rt_scene->getTopLevelAS()->getBindlessId() : 0;
	uniforms.ddgi_volume_buffer_id = GFXOPTIONS(ddgi).enabled ? ddgi_renderer.getVolumeBufferId() : 0;
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
		if (render_culling_freeze)
			return;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/frame_cull_instances.hlsl", COMPUTE_SHADER, "CS_CullInstances"));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t frustums_buffer_id;
			uint32_t frustums_count;
			uint32_t instances_count;
			uint32_t instances_pass_mask_buffer_id;
		} constants;

		constants.frustums_buffer_id = frustums_table.getBindlessIndex();
		constants.frustums_count = frustums.size();
		constants.instances_count = indirect_draw_calls_max_count;
		constants.instances_pass_mask_buffer_id = resources.getReadWriteBuffer(GFXRID(InstancesPassMask));

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		int num_groups = ceil(constants.instances_count / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
	});
}
