#include "Model.h"
#include "vk/Device.h"

namespace roing {
	Model::Model(
	        VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices,
	        const std::vector<uint32_t> &indices
	)
	    : m_Device{device.GetHandle()} {
		CreateVertexBuffer(physicalDevice, device, vertices);
		CreateIndexBuffer(physicalDevice, device, indices);

		m_IndexCount = indices.size();
	}

	Model::~Model() noexcept {
		if (m_Device == VK_NULL_HANDLE)
			return;

		vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
		vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);

		vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
		vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
	}

	void Model::CreateVertexBuffer(
	        VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices
	) {
		VkDeviceSize bufferSize{sizeof(Vertex) * vertices.size()};

		VkBuffer       stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
		        stagingBufferMemory
		);

		void *data;
		vkMapMemory(device.GetHandle(), stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), bufferSize);
		vkUnmapMemory(device.GetHandle(), stagingBufferMemory);

		device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory
		);

		device.CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

		vkDestroyBuffer(device.GetHandle(), stagingBuffer, nullptr);
		vkFreeMemory(device.GetHandle(), stagingBufferMemory, nullptr);
	}

	void Model::CreateIndexBuffer(
	        VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<uint32_t> &indices
	) {
		VkDeviceSize bufferSize{sizeof(indices[0]) * indices.size()};

		VkBuffer       stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
		        stagingBufferMemory
		);

		void *data;
		vkMapMemory(device.GetHandle(), stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), bufferSize);
		vkUnmapMemory(device.GetHandle(), stagingBufferMemory);

		device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory
		);

		device.CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

		vkDestroyBuffer(device.GetHandle(), stagingBuffer, nullptr);
		vkFreeMemory(device.GetHandle(), stagingBufferMemory, nullptr);
	}

	void Model::DrawModel(VkCommandBuffer commandBuffer) const {
		std::array<VkBuffer, 1>     vertexBuffers{m_VertexBuffer};
		std::array<VkDeviceSize, 1> offsets{0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers.data(), offsets.data());

		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(commandBuffer, m_IndexCount, 1, 0, 0, 0);
	}
}// namespace roing
