#version 460
#extension GL_EXT_ray_tracing: require
#extension GL_EXT_nonuniform_qualifier: enable
#extension GL_EXT_scalar_block_layout: enable
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64: require
#extension GL_EXT_buffer_reference2: require

#include "../src/vk/HostDevice.h"
#include "raycommon.glsl"

layout (location = 0) rayPayloadInEXT HitPayload prd;
layout (buffer_reference, scalar) buffer Vertices {Vertex v[]; };
layout (buffer_reference, scalar) buffer Indices {ivec3 i[]; };
layout (set = 0, binding = eObjDescs, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
hitAttributeEXT vec2 attribs;

layout (push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

vec3 computeDiffuse(vec3 lightDir, vec3 normal)
{
    float dotNormalLight = max(dot(normal, lightDir), 0.0);
    vec3 c = vec3(0.588, 0.588, 0.588) * dotNormalLight;

    return c;
}

void main()
{
    ObjDesc objResource = objDesc.i[gl_InstanceCustomIndexEXT];
    Indices indices = Indices(objResource.indexAddress);
    Vertices vertices = Vertices(objResource.vertexAddress);

    ivec3 ind = indices.i[gl_PrimitiveID];

    Vertex v0 = vertices.v[ind.x];
    Vertex v1 = vertices.v[ind.y];
    Vertex v2 = vertices.v[ind.z];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 pos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

    const vec3 norm = v0.norm * barycentrics.x + v1.norm * barycentrics.y + v2.norm * barycentrics.z;
    const vec3 worldNorm = normalize(vec3(norm * gl_WorldToObjectEXT));

    vec3 light;
    float lightIntensity = pcRay.lightIntensity;
    float lightDistance = 10000.0;

    if (pcRay.lightType == 0) {
        vec3 lightDirection = pcRay.lightPosition - worldPos;
        lightDistance = length(lightDirection);
        lightIntensity = pcRay.lightIntensity / (lightDistance * lightDistance);
        light = normalize(lightDirection);
    } else {
        light = normalize(pcRay.lightPosition);
    }

    vec3 diffuse = computeDiffuse(light, worldNorm);

    prd.hitValue = lightIntensity * diffuse;
}
