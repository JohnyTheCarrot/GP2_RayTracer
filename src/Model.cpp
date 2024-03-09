#include "Model.h"
#include "vk/Buffer.h"
#include "vk/Device.h"

namespace roing {
	Model::Model(
	        VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices,
	        const std::vector<uint32_t> &indices
	)
	    : m_Device{device.GetHandle()} {
		CreateVertexBuffer(physicalDevice, device, vertices);
		CreateIndexBuffer(physicalDevice, device, indices);

		m_VertexCount = vertices.size();
		m_IndexCount  = indices.size();
	}

	void Model::CreateVertexBuffer(
	        VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<Vertex> &vertices
	) {
		VkDeviceSize bufferSize{sizeof(Vertex) * vertices.size()};

		vk::Buffer stagingBuffer{device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		)};

		void *data;
		vkMapMemory(device.GetHandle(), stagingBuffer.GetMemory(), 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), bufferSize);
		vkUnmapMemory(device.GetHandle(), stagingBuffer.GetMemory());

		m_VertexBuffer = device.CreateBuffer(
		        physicalDevice, bufferSize,
		        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
		                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		device.CopyBuffer(stagingBuffer.GetHandle(), m_VertexBuffer.GetHandle(), bufferSize);
	}

	void Model::CreateIndexBuffer(
	        VkPhysicalDevice physicalDevice, vk::Device &device, const std::vector<uint32_t> &indices
	) {
		VkDeviceSize bufferSize{sizeof(indices[0]) * indices.size()};

		vk::Buffer stagingBuffer{device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		)};

		void *data;
		vkMapMemory(device.GetHandle(), stagingBuffer.GetMemory(), 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), bufferSize);
		vkUnmapMemory(device.GetHandle(), stagingBuffer.GetMemory());

		m_IndexBuffer = device.CreateBuffer(
		        physicalDevice, bufferSize,
		        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
		                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		device.CopyBuffer(stagingBuffer.GetHandle(), m_IndexBuffer.GetHandle(), bufferSize);
	}

	void Model::DrawModel(VkCommandBuffer commandBuffer) const {
		std::array<VkBuffer, 1>     vertexBuffers{m_VertexBuffer.GetHandle()};
		std::array<VkDeviceSize, 1> offsets{0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers.data(), offsets.data());

		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(commandBuffer, m_IndexCount, 1, 0, 0, 0);
	}

	BlasInput Model::ObjectToVkGeometry() const noexcept {
		VkDeviceAddress vertexBufferAddress{m_VertexBuffer.GetDeviceAddress()};
		VkDeviceAddress indexBufferAddress{m_IndexBuffer.GetDeviceAddress()};

		uint32_t maxPrimitiveCount{m_IndexCount / 3};

		VkAccelerationStructureGeometryTrianglesDataKHR triangles{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR
		};
		triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData.deviceAddress = vertexBufferAddress;
		triangles.vertexStride             = sizeof(Vertex);
		triangles.indexType                = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress  = indexBufferAddress;
		triangles.maxVertex                = m_IndexCount - 1;

		VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeom.geometry.triangles = triangles;

		VkAccelerationStructureBuildRangeInfoKHR offset{};
		offset.firstVertex     = 0;
		offset.primitiveCount  = maxPrimitiveCount;
		offset.primitiveOffset = 0;
		offset.transformOffset = 0;

		BlasInput blasInput{};
		blasInput.accStructGeometry.emplace_back(asGeom);
		blasInput.accStructBuildOffsetInfo.emplace_back(offset);

		return blasInput;
	}
}// namespace roing
