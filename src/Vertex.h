#ifndef PORTAL2RAYTRACED_VERTEX_H
#define PORTAL2RAYTRACED_VERTEX_H

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace roing {
	struct Vertex {
		glm::vec3 pos{};
		glm::vec3 color{};
	};

	constexpr VkVertexInputBindingDescription VERTEX_BINDING_DESCRIPTION{
	        .binding   = 0,
	        .stride    = sizeof(Vertex),
	        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	constexpr std::array<VkVertexInputAttributeDescription, 2> VERTEX_ATTRIBUTE_DESCRIPTIONS{
	        VkVertexInputAttributeDescription{
	                .location = 0,
	                .binding  = 0,
	                .format   = VK_FORMAT_R32G32B32_SFLOAT,
	                .offset   = offsetof(Vertex, pos),
	        },
	        VkVertexInputAttributeDescription{
	                .location = 1,
	                .binding  = 0,
	                .format   = VK_FORMAT_R32G32B32_SFLOAT,
	                .offset   = offsetof(Vertex, color),
	        }
	};
}// namespace roing

#endif//PORTAL2RAYTRACED_VERTEX_H
