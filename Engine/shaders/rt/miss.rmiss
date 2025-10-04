#extension GL_EXT_ray_tracing : enable
#include "rt/rt_common.h"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.color = vec3(0, 0, 0);
}