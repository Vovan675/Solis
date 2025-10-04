#include "pch.h"
#include "PhysXWrapper.h"

using namespace physx;

void PhysXWrapper::init()
{
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, error_callback);
	// TODO: add pvd (debugger)
	physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale(), true, nullptr);

	dispatcher = PxDefaultCpuDispatcherCreate(2);
	PxSceneDesc scene_desc(physics->getTolerancesScale());
	scene_desc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

	scene_desc.cpuDispatcher = dispatcher;
	scene_desc.filterShader = PxDefaultSimulationFilterShader;
}

void PhysXWrapper::shutdown()
{
	PX_RELEASE(dispatcher);
	PX_RELEASE(physics);
	PX_RELEASE(foundation);
}

glm::quat fromPXQuat(physx::PxQuat q)
{
	return glm::quat(q.w, q.x, q.y, q.z);
}

glm::vec3 fromPXVec(physx::PxVec3 v)
{
	return glm::vec3(v.x, v.y, v.z);
}

glm::vec4 fromPXVec(physx::PxVec4 v)
{
	return glm::vec4(v.x, v.y, v.z, v.w);
}

physx::PxVec3 toPXVec(glm::vec3 v)
{
	return physx::PxVec3(v.x, v.y, v.z);
}

physx::PxVec4 toPXVec(glm::vec4 v)
{
	return physx::PxVec4(v.x, v.y, v.z, v.w);
}
