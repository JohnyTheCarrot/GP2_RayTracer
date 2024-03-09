#include "Application.h"
#include "src/vk/PhysicalDeviceUtils.h"
#include "tiny_obj_loader.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing {
	void Application::Run() {
		InitVulkan();
		MainLoop();
	}

	void Application::InitVulkan() {
		const auto &model{m_Models.emplace_back(m_PhysicalDevice, m_Device, VERTICES, INDICES)};
		m_ModelInstances.emplace_back(&model, glm::mat4(1.0f), 0);

		const auto &tractorModel{LoadModel("tractor.obj")};
		m_ModelInstances.emplace_back(&tractorModel, glm::mat4(1.0f), 0);

		InitRayTracing();
		m_Device.SetUpRaytracing(m_PhysicalDevice, m_Models, m_ModelInstances);
		CreateRtDescriptorSet();
	}

	void Application::MainLoop() {
		while (!glfwWindowShouldClose(m_Window.Get())) {
			glfwPollEvents();

			if (m_Window.WasResized())
				UpdateRtDescriptorSet();

			DrawFrame();
		}

		vkDeviceWaitIdle(m_Device.GetHandle());
	}

	void Application::DrawFrame() {
		if (m_Device.GetSwapchain().DrawFrame(m_Window, m_Models))
			m_Device.RecreateSwapchain(m_Window, m_Surface, m_PhysicalDevice);
	}

	void Application::InitRayTracing() {
		VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		properties2.pNext = &m_RtProperties;
		vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &properties2);
	}

	void Application::CreateRtDescriptorSet() {
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<int>(RtxBindings::eTlas), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
		        VK_SHADER_STAGE_RAYGEN_BIT_KHR
		);
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<int>(RtxBindings::eOutImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
		        VK_SHADER_STAGE_RAYGEN_BIT_KHR
		);

		m_DescPool      = CreatePool();
		m_DescSetLayout = CreateDescriptorSetLayout(0);

		VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocateInfo.descriptorPool     = m_DescPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts        = &m_DescSetLayout;
		vkAllocateDescriptorSets(m_Device.GetHandle(), &allocateInfo, &m_DescSet);

		VkAccelerationStructureKHR                   tlas{m_Device.GetTlas().accStructHandle};
		VkWriteDescriptorSetAccelerationStructureKHR descASInfo{
		        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
		};
		descASInfo.accelerationStructureCount = 1;
		descASInfo.pAccelerationStructures    = &tlas;

		// TODO: taking a bet here with the image view
		VkDescriptorImageInfo imageInfo{{}, m_Device.GetSwapchain().GetCurrentImageView(), VK_IMAGE_LAYOUT_GENERAL};

		std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
		writeDescriptorSets.emplace_back(MakeWrite(m_DescSet, static_cast<uint32_t>(RtxBindings::eTlas), &descASInfo));
		writeDescriptorSets.emplace_back(MakeWrite(m_DescSet, static_cast<uint32_t>(RtxBindings::eOutImage), &imageInfo)
		);
		vkUpdateDescriptorSets(
		        m_Device.GetHandle(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0,
		        nullptr
		);

		// Camera matrices
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<uint32_t>(SceneBindings::eGlobals), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
		        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR
		);

		// Obj descriptions
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<uint32_t>(SceneBindings::eObjDescs), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
		        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
		);

		// TODO: Textures

		const VkBufferUsageFlags flags{VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
		const VkBufferUsageFlags rayTracingFlags{
		        flags | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		};
	}

	void Application::UpdateRtDescriptorSet() {
		VkDescriptorImageInfo imageInfo{{}, m_Device.GetSwapchain().GetCurrentImageView(), VK_IMAGE_LAYOUT_GENERAL};
		VkWriteDescriptorSet  writeDescriptorSet{
                MakeWrite(m_DescSet, static_cast<uint32_t>(RtxBindings::eOutImage), &imageInfo)
        };
		vkUpdateDescriptorSets(m_Device.GetHandle(), 1, &writeDescriptorSet, 0, nullptr);
	}

	void Application::AddRequiredPoolSizes(std::vector<VkDescriptorPoolSize> &poolSizes, uint32_t numSets) {
		for (const auto &binding: m_DescriptorSetLayoutBindings) {
			if (binding.descriptorCount == 0)
				continue;

			bool found{false};
			for (VkDescriptorPoolSize &poolSize: poolSizes) {
				if (poolSize.type == binding.descriptorType) {
					poolSize.descriptorCount += binding.descriptorCount * numSets;
					found = true;
					break;
				}
			};

			if (found)
				continue;

			VkDescriptorPoolSize poolSize{};
			poolSize.type            = binding.descriptorType;
			poolSize.descriptorCount = binding.descriptorCount * numSets;
			poolSizes.emplace_back(poolSize);
		}
	}

	VkDescriptorPool Application::CreatePool(uint32_t maxSets, VkDescriptorPoolCreateFlags flags) {
		std::vector<VkDescriptorPoolSize> poolSizes{};
		AddRequiredPoolSizes(poolSizes, maxSets);

		VkDescriptorPool           descriptorPool;
		VkDescriptorPoolCreateInfo descPoolInfo{};
		descPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolInfo.pNext         = nullptr;
		descPoolInfo.maxSets       = maxSets;
		descPoolInfo.poolSizeCount = poolSizes.size();
		descPoolInfo.pPoolSizes    = poolSizes.data();
		descPoolInfo.flags         = flags;

		VkResult result{vkCreateDescriptorPool(m_Device.GetHandle(), &descPoolInfo, nullptr, &descriptorPool)};
		assert(result == VK_SUCCESS);
		return descriptorPool;
	}

	VkDescriptorSetLayout Application::CreateDescriptorSetLayout(VkDescriptorSetLayoutCreateFlags flags) {
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingsInfo{
		        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO
		};

		if (m_DescriptorBindingFlags.empty() &&
		    m_DescriptorBindingFlags.size() < m_DescriptorSetLayoutBindings.size()) {
			m_DescriptorBindingFlags.resize(m_DescriptorSetLayoutBindings.size(), 0);
		}

		bindingsInfo.bindingCount  = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		bindingsInfo.pBindingFlags = m_DescriptorBindingFlags.data();

		VkDescriptorSetLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		createInfo.bindingCount = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		createInfo.pBindings    = m_DescriptorSetLayoutBindings.data();
		createInfo.flags        = flags;
		createInfo.pNext        = m_DescriptorBindingFlags.empty() ? nullptr : &bindingsInfo;

		VkDescriptorSetLayout descriptorSetLayout;
		vkCreateDescriptorSetLayout(m_Device.GetHandle(), &createInfo, nullptr, &descriptorSetLayout);
		return descriptorSetLayout;
	}

	VkWriteDescriptorSet Application::MakeWrite(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t arrayElement) {
		VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;

		for (const auto &binding: m_DescriptorSetLayoutBindings) {
			if (binding.binding != dstBinding)
				continue;

			writeSet.descriptorCount = 1;
			writeSet.descriptorType  = binding.descriptorType;
			writeSet.dstBinding      = dstBinding;
			writeSet.dstSet          = dstSet;
			writeSet.dstArrayElement = arrayElement;
			return writeSet;
		}

		throw std::runtime_error{"failed to make descriptor set"};
	}

	VkWriteDescriptorSet Application::MakeWrite(
	        VkDescriptorSet dstSet, uint32_t dstBinding,
	        const VkWriteDescriptorSetAccelerationStructureKHR *pAccelerationStructure, uint32_t arrayElement
	) {
		VkWriteDescriptorSet descriptorSet{MakeWrite(dstSet, dstBinding, arrayElement)};
		descriptorSet.pNext = pAccelerationStructure;

		return descriptorSet;
	}

	VkWriteDescriptorSet Application::MakeWrite(
	        VkDescriptorSet dstSet, uint32_t dstBinding, const VkDescriptorImageInfo *pImageInfo, uint32_t arrayElement
	) {
		VkWriteDescriptorSet descriptorSet{MakeWrite(dstSet, dstBinding, arrayElement)};
		descriptorSet.pImageInfo = pImageInfo;

		return descriptorSet;
	}

	Model &Application::LoadModel(const std::string &fileName) {
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
			std::cerr << "TinyObjReader: " << reader.Warning();
		}

		const auto &attrib{reader.GetAttrib()};
		const auto &shapes{reader.GetShapes()};
		const auto &materials{reader.GetMaterials()};

		std::vector<Vertex>   modelVertices(attrib.vertices.size() * 3);
		std::vector<uint32_t> modelIndices{};

		for (const auto &shape: shapes) {
			size_t indexOffset{0};

			for (const auto &faceVertexCount: shape.mesh.num_face_vertices) {
				for (size_t vertex{0}; vertex < faceVertexCount; ++vertex) {
					tinyobj::index_t idx{shape.mesh.indices[indexOffset + vertex]};
					tinyobj::real_t  vx{attrib.vertices[3 * idx.vertex_index + 0]};
					tinyobj::real_t  vy{attrib.vertices[3 * idx.vertex_index + 1]};
					tinyobj::real_t  vz{attrib.vertices[3 * idx.vertex_index + 2]};

					// negative = no data
					if (idx.normal_index >= 0) {
						tinyobj::real_t nx{attrib.normals[3 * idx.normal_index + 0]};
						tinyobj::real_t ny{attrib.normals[3 * idx.normal_index + 1]};
						tinyobj::real_t nz{attrib.normals[3 * idx.normal_index + 2]};
					}

					// TODO: attrib.texcoords, tx and ty

					modelVertices.emplace_back(glm::vec3{vx, vy, vz}, glm::vec3{1.f, 1.f, 1.f});
					modelIndices.emplace_back(idx.vertex_index);
				}
				indexOffset += faceVertexCount;
			}
		}

		return m_Models.emplace_back(m_PhysicalDevice, m_Device, modelVertices, modelIndices);
	}

}// namespace roing
