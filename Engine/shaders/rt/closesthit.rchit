#extension GL_EXT_ray_tracing : enable
#include "rt/rt_common.h"

layout(binding = 6, set = 0) uniform Lights 
{
	vec4 dir_light_direction;
};

struct ObjDesc
{
  vec4 color;
  uint vertexBufferOffset;
  uint indexBufferOffset;
};

layout(binding = 3) readonly buffer ObjDescArray { ObjDesc obj_descs[]; };

struct Vertex
{
  vec3 pos;
  vec3 normal;
  vec3 tangent;
  vec2 uv;
  vec3 color;
};
layout(binding = 4) readonly buffer VerticesArray { Vertex vertices[]; };
layout(binding = 5) readonly buffer IndicesArray { uint indices[]; };

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

void main()
{ 
  ObjDesc obj_desc = obj_descs[gl_InstanceCustomIndexEXT];
  uint index_0 = indices[obj_desc.indexBufferOffset + gl_PrimitiveID * 3];
  uint index_1 = indices[obj_desc.indexBufferOffset + gl_PrimitiveID * 3 + 1];
  uint index_2 = indices[obj_desc.indexBufferOffset + gl_PrimitiveID * 3 + 2];
  Vertex v0 = vertices[obj_desc.vertexBufferOffset + index_0];
  Vertex v1 = vertices[obj_desc.vertexBufferOffset + index_1];
  Vertex v2 = vertices[obj_desc.vertexBufferOffset + index_2];

  const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  vec3 pos = v0.pos * barycentricCoords.x + v1.pos * barycentricCoords.y + v2.pos * barycentricCoords.z;
  vec3 world_pos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

  vec3 normal = v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z;
  vec3 world_normal = normalize(vec3(mat3(gl_ObjectToWorldEXT) * normal));

  //hitValue = barycentricCoords;
  //hitValue = obj_desc.color;
  //hitValue = vert.pos + vert.normal + vec3(vert.uv, 0) + vert.color;
	vec3 light_dir = normalize(dir_light_direction.xyz);

  float lighting = dot(world_normal, light_dir);
  payload.color = vec3(lighting, lighting, lighting);
  payload.color = vec3(1, 1, 1);
  payload.normal = world_normal;
 // payload.color = vec3(world_normal);
  payload.t = gl_HitTEXT;
  //hitValue = vec3(index_0 / 16.0f, 0, 0);
}