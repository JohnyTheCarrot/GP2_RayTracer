#ifndef PORTAL2RAYTRACED_MODEL_H
#define PORTAL2RAYTRACED_MODEL_H

#define GLFW_INCLUDE_VULKAN
#include "Vertex.h"
#include "vk/Buffer.h"
#include <GLFW/glfw3.h>
#include <vector>

namespace roing {

	namespace vk {
		class Device;
	}

	struct BlasInput final {
		std::vector<VkAccelerationStructureGeometryKHR>       accStructGeometry;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> accStructBuildOffsetInfo;
		VkBuildAccelerationStructureFlagsKHR                  flags{0};
	};

	class Model final {
	public:
		Model(VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices,
		      const std::vector<uint32_t> &indices);

		Model(const Model &)            = delete;
		Model &operator=(const Model &) = delete;

		Model(Model &&other) noexcept
		    : m_Device{other.m_Device}
		    , m_VertexBuffer{std::move(other.m_VertexBuffer)}
		    , m_IndexBuffer{std::move(other.m_IndexBuffer)}
		    , m_IndexCount{other.m_IndexCount}
		    , m_VertexCount{other.m_VertexCount} {
			other.m_Device = VK_NULL_HANDLE;
		}

		Model &operator=(Model &&other) noexcept {
			m_Device       = other.m_Device;
			other.m_Device = VK_NULL_HANDLE;
			m_VertexBuffer = std::move(other.m_VertexBuffer);
			m_VertexCount  = other.m_VertexCount;
			m_IndexBuffer  = std::move(other.m_IndexBuffer);
			m_IndexCount   = other.m_IndexCount;

			return *this;
		}

		void DrawModel(VkCommandBuffer commandBuffer) const;

		[[nodiscard]]
		BlasInput ObjectToVkGeometry() const noexcept;

		[[nodiscard]]
		VkDeviceAddress GetVertexBufferAddress() const noexcept;

		[[nodiscard]]
		VkDeviceAddress GetIndexBufferAddress() const noexcept;

	private:
		static vk::Buffer
		CreateVertexBuffer(VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices);
		static vk::Buffer
		CreateIndexBuffer(VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<uint32_t> &indices);

		VkDevice   m_Device{};
		vk::Buffer m_VertexBuffer;
		vk::Buffer m_IndexBuffer;
		uint32_t   m_VertexCount{};
		uint32_t   m_IndexCount{};
	};

}// namespace roing

#endif//PORTAL2RAYTRACED_MODEL_H
