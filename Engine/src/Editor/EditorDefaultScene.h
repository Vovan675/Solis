#pragma once
#include "Scene/Scene.h"
#include "Rendering/Model.h"
#include "Scene/Components.h"

static class EditorDefaultScene
{
public:
	// later it will be just .world file
	static void createScene(Camera *camera)
	{
		Entity light = Scene::getCurrentScene()->createEntity("Directional Light");
		light.getTransform().setPosition(glm::vec3(0, 1, 0));
		light.getTransform().setLocalRotationEuler(glm::radians(glm::vec3(-111.0f, 0.0f, 175.0f)));
		auto &light_component = light.addComponent<LightComponent>();
		light_component.setType(LIGHT_TYPE_DIRECTIONAL);
		//light_component.setType(LIGHT_TYPE_POINT);
		light_component.color = glm::vec3(253.0f / 255, 251.0f / 255, 211.0f / 255);
		light_component.intensity = 1.0f;
		light_component.radius = 10.0f;

		enum TestScene
		{
			SPONZA,
			LOTS_OF_DUPLICATES,
			LOTS_OF_DUPLICATES_HARD,
			SIMPLE,
			CULLING
		} scene;

		scene = LOTS_OF_DUPLICATES;

		if (scene == SPONZA)
		{
			auto model = AssetManager::getModelAsset("assets/sponza/sponza.obj");

			Entity entity = model->createEntity(model);
			entity.getTransform().setLocalScale(glm::vec3(0.003));
		} else if (scene == LOTS_OF_DUPLICATES)
		{
			auto model = AssetManager::getModelAsset("assets/sponza/sponza.obj");

			for (int x = -3; x <= 3; x++)
			{
				for (int z = 0; z < 10; z++)
				{
					Entity entity = model->createEntity(model);
					entity.getTransform().setLocalScale(glm::vec3(0.003));
					entity.getTransform().setPosition(glm::vec3(x * 15, 0, -30 + z * 10));
				}
			}
		} else if (scene == LOTS_OF_DUPLICATES_HARD)
		{
			auto model = AssetManager::getModelAsset("assets/bistro/BistroExterior.fbx");

			for (int x = -3; x <= 3; x++)
			{
				for (int z = 0; z < 10; z++)
				{
					Entity entity = model->createEntity(model);
					entity.getTransform().setLocalScale(glm::vec3(0.3));
					entity.getTransform().setPosition(glm::vec3(x * 35, 0, -30 + z * 40));
				}
			}
		} else if (scene == SIMPLE)
		{
			auto model = AssetManager::getModelAsset("assets/demo_scene.fbx");
			Entity entity = model->createEntity(model);
			entity.getTransform().setLocalScale(glm::vec3(1));
		} else if (scene == CULLING)
		{
			auto model = AssetManager::getModelAsset("assets/cube.fbx");
			
			camera->setPosition(glm::vec3(0, 0, 0));
			camera->setRotation(glm::vec3(0, 0.01, 0.01));
			
			Entity wall = model->createEntity(model);
			wall.getTransform().setLocalScale(glm::vec3(0.01));
			wall.getTransform().setPosition(glm::vec3(0, 0, -2));
			
			for (int x = -2; x <= 2; x++)
			{
				for (int y = -1; y <= 1; y++)
				{
					Entity cube = model->createEntity(model);
					cube.getTransform().setLocalScale(glm::vec3(0.002));
					cube.getTransform().setPosition(glm::vec3(x * 0.5f, y * 0.5f, -5));
				}
			}
		}

		//return;
		//Scene::getCurrentScene()->loadFile("assets/demo_scene.scene");
		//Scene::getCurrentScene()->loadFile("assets/cerberus/cerberus.scene");
		//return;
		// Demo Scene
		//auto model = AssetManager::getModelAsset("assets/demo_scene.fbx");
		//auto model = AssetManager::getModelAsset("assets/cube.fbx");
		//auto model = AssetManager::getModelAsset("assets/game/map.fbx");
		//auto model = AssetManager::getModelAsset("assets/sponza/sponza.obj");
		//auto model = AssetManager::getModelAsset("assets/big_city_2/scene.gltf");
		//auto model = AssetManager::getModelAsset("assets/new_sponza/NewSponza_Main_Yup_002.fbx");
		//auto model = AssetManager::getModelAsset("assets/bistro/BistroExterior.fbx");
		//auto model = AssetManager::getModelAsset("assets/hideout/source/FullSceneSubstance.fbx");
		//auto model = AssetManager::getModelAsset("assets/pbr/source/Ref.fbx");
		//auto model = AssetManager::getModelAsset("assets/level/Isometric_Game_Level_Low_Poly.obj");
		//auto model = AssetManager::getModelAsset("assets/other_sponza/Sponza.gltf");
		//auto model = AssetManager::getModelAsset("assets/pica/scene.gltf");
		//auto model = AssetManager::getModelAsset("assets/axis.fbx");
		//model->saveFile("test_model.mesh");
		//model->loadFile("test_model.mesh");
		//Entity entity = model->createEntity(model);
		//entity.getTransform().setLocalScale(glm::vec3(1));
		//entity.getTransform().setLocalScale(glm::vec3(0.01));

		//auto mesh_ball = std::make_shared<Engine::Mesh>("assets/ball.fbx");
		int count_x = 5;
		int count_y = 5;
		/*
		for (int x = 0; x < count_x; x++)
		{
		for (int y = 0; y < count_y; y++)
		{
		//auto entity = std::make_shared<Entity>("assets/ball.fbx");
		//auto entity_renderer = std::make_shared<MeshRenderer>(camera, entity);
		auto entity_renderer = std::make_shared<MeshRenderer>(mesh_ball);
		//entity_renderer->setLocalTransform(glm::scale(glm::mat4(1.0), glm::vec3(0.001f)) * glm::translate(glm::mat4(1.0), glm::vec3(-2.0 * 5 + 2.0 * x, 2.0 * y, 0)));
		entity_renderer->setPosition(glm::vec3(-0.3 * 4 + 0.3 * x, 0.3 * y, 0));
		entity_renderer->setLocalScale(glm::vec3(0.001f));
		renderers.push_back(entity_renderer);
		entities_renderers.push_back(entity_renderer);

		Material mat;
		mat.metalness = y / (float)(count_y - 1);
		mat.roughness = x / (float)(count_x - 1);
		mat.albedo = glm::vec4(1, 0, 0, 1);
		entity_renderer->setMaterial(mat);
		}
		}
		*/
	}
};