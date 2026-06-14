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
		this->scene->registry.on_construct<MeshRendererComponent>().disconnect<&SceneRenderer::on_mesh_added>(this);
		this->scene->registry.on_update<MeshRendererComponent>().disconnect<&SceneRenderer::on_mesh_added>(this);
	}

	this->scene = scene;
	if (engine_ray_tracing)
	{
		rt_scene = new RayTracingScene(scene);
	} else
	{
		rt_scene = nullptr;
	}

	if (!scene)
		return;

	for (entt::entity entity : scene->registry.view<MeshRendererComponent>())
		on_mesh_added(scene->registry, entity);

	scene->registry.on_construct<MeshRendererComponent>().connect<&SceneRenderer::on_mesh_added>(this);
	scene->registry.on_update<MeshRendererComponent>().connect<&SceneRenderer::on_mesh_added>(this);
}

void SceneRenderer::on_mesh_added(entt::registry &registry, entt::entity entity)
{
	auto &mesh_renderer = registry.get<MeshRendererComponent>(entity);
	for (int i = 0; i < mesh_renderer.meshes.size(); i++)
	{
		Engine::Mesh *mesh = mesh_renderer.meshes[i].getMesh();
		if (mesh == nullptr)
			continue;
		const Engine::MeshletFileView *file_view = mesh_renderer.meshes[i].model->getFileView(mesh->id);
		geometry_streaming.registerMesh(mesh, *file_view);
	}
}

void SceneRenderer::on_asset_pre_reimport(Asset *asset)
{
	if (!scene || asset->getAssetType() != ASSET_TYPE_MODEL)
		return;

	Model *model = static_cast<Model *>(asset);
	eastl::vector<Ref<Engine::Mesh>> old_meshes;
	model->getMeshes(old_meshes);
	for (auto &old_mesh : old_meshes)
		geometry_streaming.unregisterMesh(old_mesh);
}

void SceneRenderer::on_asset_post_reimport(Asset *asset)
{
	if (!scene)
		return;

	if (asset->getAssetType() == ASSET_TYPE_TEXTURE)
	{
		auto view = scene->registry.view<MeshRendererComponent>();
		for (entt::entity entity : view)
		{
			MeshRendererComponent &mesh_renderer = view.get<MeshRendererComponent>(entity);
			for (Ref<Material> &material : mesh_renderer.materials)
				material->invalidateTextures();
		}
		render_first_frame = true; // TODO: remove from here, should be reactive
	} else if (asset->getAssetType() == ASSET_TYPE_MODEL)
	{
		Model *model = static_cast<Model *>(asset);
		auto view = scene->registry.view<MeshRendererComponent>();
		for (entt::entity entity : view)
		{
			MeshRendererComponent &mesh_renderer = view.get<MeshRendererComponent>(entity);
			for (auto &mesh_id : mesh_renderer.meshes)
			{
				if (mesh_id.model == model)
				{
					on_mesh_added(scene->registry, entity);
					break;
				}
			}
		}
		render_first_frame = true; // TODO: remove from here, should be reactive
	}
}

void SceneRenderer::render(Camera *camera, RHITextureRef result_texture)
{
	PROFILE_CPU_FUNCTION();
	PROFILE_GPU_FUNCTION(gDynamicRHI->getCmdList());

	Renderer::setViewportSize(result_texture->getSize());
	Renderer::setCamera(camera);

	gUploadManager->beginFrame();

	update(camera);

	FrameGraph frame_graph;

	frame_graph.importTexture(GFXRID(FinalTexture), result_texture);
	frame_graph.importTexture(GFXRID(LutBRDF), lut_renderer.brdf_lut_texture);
	geometry_streaming.importBuffers(frame_graph);

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

	geometry_streaming.addAgeFilterAndReadbackPasses(frame_graph);

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

	auto fill_buffer = [](RHIBufferRef &buffer, void *data, size_t count, size_t stride, const char *name, BufferUsage usage = BufferUsage::SHADER_READ_BUFFER, bool use_staging = true)
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

		{
			uint32_t frustums_size = frustums.size() * sizeof(FrustumDataGPU);
			if (!frustums_gpu || frustums_gpu->getSize() < frustums_size)
			{
				BufferDescription desc;
				desc.size = frustums_size;
				desc.usage = BufferUsage::SHADER_READ_BUFFER;
				desc.use_staging_buffer = true;
				desc.storage_stride = sizeof(FrustumDataGPU);
				frustums_gpu = gDynamicRHI->createBuffer(desc);
				frustums_gpu->setDebugName("Frustums Buffer");
			}
			BufferDescription staging_desc;
			staging_desc.size = frustums_size;
			staging_desc.usage = BufferUsage::STAGING_BUFFER;
			staging_desc.use_staging_buffer = false;
			staging_desc.storage_stride = sizeof(FrustumDataGPU);
			RHIBufferRef staging = gDynamicRHI->createBuffer(staging_desc);
			staging->fill(frustums.data());
			RHICommandList *cmd_list = gDynamicRHI->getCmdList();
			cmd_list->copyBuffer(staging, frustums_gpu, 0, 0, frustums_size);
			frustums_gpu->transitState(ResourceState::SHADER_RESOURCE);
		}

		geometry_streaming.update();

		// TODO: replace with dirty tracking + upload in future
		static bool gpu_buffers_uploaded = false;
		if (!gpu_buffers_uploaded || render_first_frame)
		{
			auto entities_view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();

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

					Material *material = mesh_renderer.materials[i];
					material->update();

					RenderObject &render_object = render_objects.emplace_back();
					render_object.transform = &transform;
					render_object.mesh = mesh;
					render_object.material = material;
					const Engine::MeshletFileView *file_view = mesh_renderer.meshes[i].model->getFileView(mesh->id);
					if (file_view)
						render_object.file_view = *file_view;
				}
			}

			materials.clear();
			meshes.clear();
			instances.clear();
			instances_pass_masks.clear();

			for (int i = 0; i < render_objects.size(); i++)
			{
				RenderObject &render_object = render_objects[i];
				TransformComponent *transform = render_object.transform;
				Engine::Mesh *mesh = render_object.mesh;
				Material *material = render_object.material;

				InstanceGPU &instance = instances.emplace_back();
				instance.world_transform = transform->getWorldTransform();
				instance.iworld_transform = transform->getInverseWorldTransform();
				instance.mesh_id = meshes.size();
				instance.material_id = materials.size();

				BoundBox bound_box(mesh->bound_box);
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

				/*
				for (const auto &group : mesh->meshlet_geometry->meshlet_lod_groups)
				{
					glm::vec4 color = glm::vec4(group.depth / 10.0f, 0, 0, 1);
					debug_renderer.addSphere(group.center, group.radius, 32, color);
				}
				*/


				#if 0
				// check how traversal works
				struct Item
				{
					bool is_node;
					uint32_t child_id;
					uint32_t group_id;
					uint32_t count;
				};

				LodNode root_node = mesh->meshlet_geometry->lod_nodes[mesh->meshlet_geometry->meshlet_root_group_local_offset];

				eastl::queue<Item> items;

				// Starting as root node was pushed
				Item &it = items.emplace();
				it.is_node = true;
				it.child_id = root_node.first_child;
				it.count = root_node.child_count;

				while (items.size())
				{
					Item item = items.front();
					items.pop();

					// Traverse sub items of item
					for (int local_sub_item_id = 0; local_sub_item_id < item.count; local_sub_item_id++)
					{

						// Traverse hierarchy of nodes
						if (item.is_node)
						{
							LodNode lod_node = mesh->meshlet_geometry->lod_nodes[item.child_id + local_sub_item_id];

							bool is_child_group = lod_node.child_count == 0;

							Item &i = items.emplace();
							i = {};

							if (is_child_group)
							{
								i.is_node = false;
								i.group_id = lod_node.group_index;
								i.count = lod_node.meshlet_count;
							} else
							{
								i.is_node = true;
								i.child_id = lod_node.first_child;
								i.count = lod_node.child_count;
							}

						}
						// Meshlets level
						else
						{
							LODGroup lod_group = mesh->meshlet_geometry->meshlet_lod_groups[item.group_id];
							uint32_t meshlet_id = lod_group.first_meshlet + local_sub_item_id;

						}
					}
				}

				#endif
			}

			indirect_draw_calls_max_count = instances.size();

			fill_buffer(materials_gpu, materials.data(), materials.size(), sizeof(MaterialGPU), "Materials Buffer");
			fill_buffer(instances_gpu, instances.data(), instances.size(), sizeof(InstanceGPU), "Instances Buffer");
			fill_buffer(meshes_gpu, meshes.data(), meshes.size(), sizeof(MeshGPU), "Meshes Buffer");

			gpu_buffers_uploaded = true;
		}

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

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/frame_cull_instances.hlsl", COMPUTE_SHADER, "CS_CullInstances"));
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
		constants.instances_count = indirect_draw_calls_max_count;
		constants.instances_pass_mask_buffer_id = resources.getReadWriteBuffer(GFXRID(InstancesPassMask));

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		int num_groups = ceil(constants.instances_count / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
	});
}
