#ifndef PORTAL2RAYTRACED_HOSTDEVICE_H
#define PORTAL2RAYTRACED_HOSTDEVICE_H

#ifdef __cplusplus
#include <glm/glm.hpp>
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

// clang-format off
#ifdef __cplusplus
#define ENUM(name) enum name {
#define END_ENUM() }
#else
#define ENUM(name) const uint
#define END_ENUM()
#endif

ENUM(RtxBindings) eTlas = 0, eOutImage = 1, eGlobals = 2, eObjDescs = 3, eTextures = 4, eImplicits = 5 END_ENUM();

// clang-format on

struct GlobalUniforms {
	mat4 viewProj;
	mat4 viewInverse;
	mat4 projInverse;
};

struct PushConstantRay {
	vec4  clearColor;
	vec3  lightPosition;
	float lightIntensity;
	int   lightType;
};

struct ObjDesc {
	int      txtOffset;
	uint64_t vertexAddress;
	uint64_t indexAddress;
};

#endif//PORTAL2RAYTRACED_HOSTDEVICE_H
