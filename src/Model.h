#ifndef PORTAL2RAYTRACED_MODEL_H
#define PORTAL2RAYTRACED_MODEL_H

#define GLFW_INCLUDE_VULKAN
#include "Vertex.h"
#include <GLFW/glfw3.h>
#include <vector>

namespace roing {

	namespace vk {
		class Device;
	}

	class Model final {
	public:
		Model() = default;

		Model(VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices,
		      const std::vector<uint32_t> &indices);

		~Model() noexcept;

		Model(const Model &)            = delete;
		Model &operator=(const Model &) = delete;

		Model(Model &&other) noexcept
		    : m_Device{other.m_Device}
		    , m_VertexBuffer{other.m_VertexBuffer}
		    , m_VertexBufferMemory{other.m_VertexBufferMemory}
		    , m_IndexBuffer{other.m_IndexBuffer}
		    , m_IndexBufferMemory{other.m_IndexBufferMemory} {
			other.m_Device = VK_NULL_HANDLE;
		}

		Model &operator=(Model &&other) noexcept {
			m_Device             = other.m_Device;
			other.m_Device       = VK_NULL_HANDLE;
			m_VertexBuffer       = other.m_VertexBuffer;
			m_VertexBufferMemory = other.m_VertexBufferMemory;
			m_IndexBuffer        = other.m_IndexBuffer;
			m_IndexBufferMemory  = other.m_IndexBufferMemory;

			return *this;
		}

		void DrawModel(VkCommandBuffer commandBuffer) const;

	private:
		void
		CreateVertexBuffer(VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices);
		void
		CreateIndexBuffer(VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<uint32_t> &indices);

		VkDevice       m_Device{};
		VkBuffer       m_VertexBuffer{};
		VkDeviceMemory m_VertexBufferMemory{};
		VkBuffer       m_IndexBuffer{};
		VkDeviceMemory m_IndexBufferMemory{};
		uint32_t       m_IndexCount{};
	};

}// namespace roing

#endif//PORTAL2RAYTRACED_MODEL_H
