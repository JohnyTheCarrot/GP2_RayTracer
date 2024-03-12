#include "Model.h"
#include "tiny_obj_loader.h"
#include "vk/Buffer.h"
#include "vk/CommandPool.h"
#include "vk/Device.h"
#include <iostream>

namespace roing {
	Model::Model(
	        VkPhysicalDevice physicalDevice, const vk::Device &device, const vk::CommandPool &commandPool,
	        const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices
	)
	    : m_Device{device.GetHandle()}
	    , m_VertexBuffer{CreateVertexBuffer(physicalDevice, device, commandPool, vertices)}
	    , m_IndexBuffer{CreateIndexBuffer(physicalDevice, device, commandPool, indices)}
	    , m_VertexCount{static_cast<uint32_t>(vertices.size())}
	    , m_IndexCount{static_cast<uint32_t>(indices.size())} {
	}

	vk::Buffer Model::CreateVertexBuffer(
	        VkPhysicalDevice physicalDevice, const vk::Device &device, const vk::CommandPool &commandPool,
	        const std::vector<Vertex> &vertices
	) {
		VkDeviceSize bufferSize{sizeof(Vertex) * vertices.size()};

		vk::Buffer stagingBuffer{device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		)};

		void *data;
		vkMapMemory(device.GetHandle(), stagingBuffer.GetMemoryHandle(), 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), bufferSize);
		vkUnmapMemory(device.GetHandle(), stagingBuffer.GetMemoryHandle());

		vk::Buffer buffer{device.CreateBuffer(
		        physicalDevice, bufferSize,
		        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		)};

		device.CopyBuffer(commandPool, stagingBuffer.GetHandle(), buffer.GetHandle(), bufferSize);

		return buffer;
	}

	vk::Buffer Model::CreateIndexBuffer(
	        VkPhysicalDevice physicalDevice, vk::Device &device, const vk::CommandPool &commandPool,
	        const std::vector<uint32_t> &indices
	) {
		VkDeviceSize bufferSize{sizeof(indices[0]) * indices.size()};

		vk::Buffer stagingBuffer{device.CreateBuffer(
		        physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		)};

		void *data;
		vkMapMemory(device.GetHandle(), stagingBuffer.GetMemoryHandle(), 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), bufferSize);
		vkUnmapMemory(device.GetHandle(), stagingBuffer.GetMemoryHandle());

		vk::Buffer buffer{device.CreateBuffer(
		        physicalDevice, bufferSize,
		        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		)};

		device.CopyBuffer(commandPool, stagingBuffer.GetHandle(), buffer.GetHandle(), bufferSize);

		return buffer;
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
		triangles.maxVertex                = m_VertexCount - 1;

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

	VkDeviceAddress Model::GetVertexBufferAddress() const noexcept {
		return m_VertexBuffer.GetDeviceAddress();
	}

	VkDeviceAddress Model::GetIndexBufferAddress() const noexcept {
		return m_IndexBuffer.GetDeviceAddress();
	}

	Model Model::LoadModel(
	        const vk::Device &device, const vk::CommandPool &commandPool, VkPhysicalDevice physicalDevice,
	        const std::string &fileName
	) {
		tinyobj::ObjReaderConfig readerConfig{};
		readerConfig.mtl_search_path = "./";

		tinyobj::ObjReader reader;

		if (!reader.ParseFromFile("resources/" + fileName, readerConfig)) {
			if (!reader.Error().empty()) {
				throw std::runtime_error(reader.Error());
			}
			throw std::runtime_error("failed to parse obj file");
		}

		if (!reader.Warning().empty()) {
			std::cerr << "TinyObjReader warning: " << reader.Warning();
		}

		const auto &attrib{reader.GetAttrib()};
		const auto &shapes{reader.GetShapes()};
		//		const auto &materials{reader.GetMaterials()};

		std::vector<Vertex>   modelVertices{};
		std::vector<uint32_t> modelIndices{};

		for (const auto &shape: shapes) {
			modelVertices.reserve(shape.mesh.indices.size() + modelVertices.size());
			modelIndices.reserve(shape.mesh.indices.size() + modelIndices.size());

			for (const auto &index: shape.mesh.indices) {
				Vertex vertex{};
				vertex.pos.x = attrib.vertices[3 * index.vertex_index + 0];
				vertex.pos.y = attrib.vertices[3 * index.vertex_index + 1];
				vertex.pos.z = attrib.vertices[3 * index.vertex_index + 2];

				if (!attrib.normals.empty() && index.normal_index >= 0) {
					vertex.norm.x = attrib.normals[3 * index.normal_index + 0];
					vertex.norm.y = attrib.normals[3 * index.normal_index + 1];
					vertex.norm.z = attrib.normals[3 * index.normal_index + 2];
				}

				modelVertices.emplace_back(vertex);
				modelIndices.push_back(static_cast<int>(modelIndices.size()));
			}
		}

		return Model{physicalDevice, device, commandPool, modelVertices, modelIndices};
	}
}// namespace roing
