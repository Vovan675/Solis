#pragma once
#include "PxPhysicsAPI.h"

	glm::quat fromPXQuat(physx::PxQuat q);
	glm::vec3 fromPXVec(physx::PxVec3 v);
	glm::vec4 fromPXVec(physx::PxVec4 v);
	physx::PxVec3 toPXVec(glm::vec3 v);
	physx::PxVec4 toPXVec(glm::vec4 v);

class PhysXWrapper
{
public:
	static void init();
	static void shutdown();

	static physx::PxFoundation *getFoundation() { return foundation; }
	static physx::PxPhysics *getPhysics() { return physics; }
	static physx::PxDefaultCpuDispatcher *getDispatcher() { return dispatcher; }

private:
	inline static physx::PxDefaultAllocator allocator;
	inline static physx::PxDefaultErrorCallback error_callback;
	inline static physx::PxFoundation *foundation;
	inline static physx::PxPhysics *physics;
	inline static physx::PxDefaultCpuDispatcher *dispatcher;
};