#include "pch.h"
#include "PhysicsScene.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

using namespace physx;


PhysicsScene::PhysicsScene(Scene *scene)
{
	this->scene = scene;
	PxSceneDesc scene_desc(PhysXWrapper::getPhysics()->getTolerancesScale());
	scene_desc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

	scene_desc.cpuDispatcher = PhysXWrapper::getDispatcher();
	scene_desc.filterShader = PxDefaultSimulationFilterShader;
	px_scene = PhysXWrapper::getPhysics()->createScene(scene_desc);
}

PhysicsScene::~PhysicsScene()
{
	// TODO: revert
	PX_RELEASE(px_scene);
}

void PhysicsScene::simulate()
{
	px_scene->simulate(1.0 / 200.0f);
	px_scene->fetchResults(true);

	uint32_t nb_active_actors;
	//PxActor **actors = px_scene->getActiveActors(nb_active_actors);

	nb_active_actors = px_scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
	PxArray<PxRigidActor *> actors(nb_active_actors);
	px_scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC, reinterpret_cast<PxActor **>(&actors[0]), nb_active_actors);
	for (int i = 0; i < nb_active_actors; i++)
	{
		TransformComponent *t = (TransformComponent *)actors[i]->userData;
		PxRigidActor *actor = reinterpret_cast<PxRigidActor *>(actors[i]);
		const PxTransform pose = actor->getGlobalPose();
		
		t->position = fromPXVec(pose.p);
		t->setRotation(fromPXQuat(pose.q));
	}
}

void PhysicsScene::reinit()
{
	auto rbs = scene->getEntitiesWith<RigidBodyComponent>();
	for (auto [e, rb] : rbs.each())
	{
		Entity entity(e);
		//entity.getWorldTransformMatrix()
		auto &t = entity.getComponent<TransformComponent>();
		auto transform = PxTransform(t.position.x, t.position.y, t.position.z, PxQuat(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w));
		PxRigidActor *actor;
		if (rb.is_static)
			actor = PhysXWrapper::getPhysics()->createRigidStatic(transform);
		else
		{
			PxRigidDynamic *dynamic_actor = PhysXWrapper::getPhysics()->createRigidDynamic(transform);
			dynamic_actor->setMass(rb.mass);
			dynamic_actor->setLinearDamping(rb.linear_damping);
			dynamic_actor->setAngularDamping(rb.angular_damping);
			dynamic_actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, rb.is_kinematic);
			dynamic_actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, rb.gravity == false);
			actor = dynamic_actor;
		}
		actor->userData = &t;
		entity_body[e] = actor;
		px_scene->addActor(*actor);
	}

	static PxMaterial *material;
	material = PhysXWrapper::getPhysics()->createMaterial(0.5f, 0.5f, 0.6f);

	auto box_colliders = scene->getEntitiesWith<RigidBodyComponent, BoxColliderComponent>();
	for (auto [e, _,collider] : box_colliders.each())
	{
		PxShape *shape = PhysXWrapper::getPhysics()->createShape(PxBoxGeometry(collider.half_extent.x, collider.half_extent.y, collider.half_extent.z), *material);
		PxRigidActor *body = entity_body[e];
		body->attachShape(*shape);
		//PxRigidBodyExt::updateMassAndInertia(*body, 10.0f);
		shape->release();
	}
}

void PhysicsScene::draw_debug(DebugRenderer *debug_renderer)
{
	uint32_t nb_actors = px_scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
	PxArray<PxRigidActor *> actors(nb_actors);
	px_scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC, reinterpret_cast<PxActor **>(&actors[0]), nb_actors);
	for (int i = 0; i < nb_actors; i++)
	{
		TransformComponent *t = (TransformComponent *)actors[i]->userData;
		PxRigidActor *actor = reinterpret_cast<PxRigidActor *>(actors[i]);

		PxShape *shapes[128];
		uint32_t nb_shapes = actor->getNbShapes();
		actor->getShapes(shapes, nb_shapes);

		for (size_t s = 0; s < nb_shapes; s++)
		{
			const PxGeometry &geom = shapes[s]->getGeometry();
			const PxTransform pose = PxShapeExt::getGlobalPose(*shapes[s], *actor);

			if (geom.getType() == PxGeometryType::eBOX)
			{
				//actors[i]->setGlobalPose()
				//cube.getComponent<TransformComponent>().position = transform;

				const PxBoxGeometry &box = static_cast<const PxBoxGeometry &>(geom);
				glm::vec3 extents(box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);
				debug_renderer->addBox(extents, glm::translate(glm::mat4(1), fromPXVec(pose.p)) * glm::toMat4(fromPXQuat(pose.q)));
			}
		}
	}
}
