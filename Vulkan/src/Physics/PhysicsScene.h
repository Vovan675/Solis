#pragma once
#include "entt/entt.hpp"
#include "Physics/PhysXWrapper.h"
#include "Renderers/DebugRenderer.h"

class Scene;

class PhysicsScene
{
public:
	PhysicsScene(Scene *scene);
	~PhysicsScene();
	
	void simulate();
	void reinit();

	void draw_debug(DebugRenderer *debug_renderer);
private:
	Scene *scene;
	physx::PxScene *px_scene = nullptr;
	std::unordered_map<entt::entity, physx::PxRigidActor *> entity_body;
};

