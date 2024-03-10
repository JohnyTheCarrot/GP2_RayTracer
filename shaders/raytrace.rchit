#version 460
#extension GL_EXT_ray_tracing: require
#extension GL_EXT_nonuniform_qualifier: enable

#include "raycommon.glsl"

layout (location = 0) rayPayloadInEXT HitPayload prd;
hitAttributeEXT vec3 attribs;

void main()
{
    prd.hitValue = vec3(0.2, 0.5, 0.5);
}
