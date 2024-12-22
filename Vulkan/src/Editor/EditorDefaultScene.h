#pragma once
#include "Scene/Scene.h"

static class EditorDefaultScene
{
public:
	// later it will be just .world file
	static void createScene()
	{
		// Demo Scene
		//auto model = AssetManager::getModelAsset("assets/demo_scene.fbx");
		//auto model = AssetManager::getModelAsset("assets/game/map.fbx");
		//auto model = AssetManager::getModelAsset("assets/sponza/sponza.obj");
		//auto model = AssetManager::getModelAsset("assets/new_sponza/NewSponza_Main_Yup_002.fbx");
		//auto model = AssetManager::getModelAsset("assets/bistro/BistroExterior.fbx");
		//auto model = AssetManager::getModelAsset("assets/hideout/source/FullSceneSubstance.fbx");
		auto model = AssetManager::getModelAsset("assets/pbr/source/Ref.fbx");
		//auto model = AssetManager::getModelAsset("assets/level/Isometric_Game_Level_Low_Poly.obj");
		//model->saveFile("test_model.mesh");
		//model->loadFile("test_model.mesh");
		Entity entity = model->createEntity(model);
		entity.getTransform().scale = glm::vec3(0.01);
		//entity.addComponent<LightComponent>();

		Entity light = Scene::getCurrentScene()->createEntity("Point Light");
		light.getTransform().setTransform(glm::eulerAngleXYX(3.14 / 4.0, 3.14 / 4.0, 0.0));
		//light.getTransform().setTransform(glm::eulerAngleXYX(3.14 / 2.0f, 0.01, 0.0));
		auto &light_component = light.addComponent<LightComponent>();
		light_component.setType(LIGHT_TYPE_DIRECTIONAL);
		light_component.color = glm::vec3(253.0f / 255, 251.0f / 255, 211.0f / 255);
		light_component.intensity = 1.0f;
		light_component.radius = 10.0f;

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
		//entity_renderer->setTransform(glm::scale(glm::mat4(1.0), glm::vec3(0.001f)) * glm::translate(glm::mat4(1.0), glm::vec3(-2.0 * 5 + 2.0 * x, 2.0 * y, 0)));
		entity_renderer->setPosition(glm::vec3(-0.3 * 4 + 0.3 * x, 0.3 * y, 0));
		entity_renderer->setScale(glm::vec3(0.001f));
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