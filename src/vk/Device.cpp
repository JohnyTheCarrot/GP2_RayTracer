#include "Device.h"
#include "HostDevice.h"
#include "Instance.h"
#include "PhysicalDeviceUtils.h"
#include "QueueFamilyIndices.h"
#include "Window.h"
#include "src/tiny_obj_loader.h"
#include "src/utils/ToTransformMatrixKHR.h"
#include "src/utils/debug.h"
#include "src/utils/read_file.h"
#include <set>
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Device::~Device() noexcept {
		StartDestruction("Device");
		m_Models.clear();
		m_ModelInstances.clear();

		vkDestroyRenderPass(GetHandle(), m_RenderPass, nullptr);
		vkDestroyPipeline(GetHandle(), m_PostPipeline, nullptr);
		m_AccelerationStructureBuffers.clear();

		EndDestruction("Device");
	}

	Device::Device(const Surface &surface, VkPhysicalDevice physicalDevice, const Window &window)
	    : m_QueueFamilyIndices{physical_device::FindQueueFamilies(surface, physicalDevice)} {
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
		// todo: use unordered_set?
		std::set<uint32_t> uniqueQueueFamilies{
		        m_QueueFamilyIndices.graphicsFamily.value(), m_QueueFamilyIndices.presentFamily.value()
		};

		float queuePriority = 1.f;
		for (uint32_t queueFamily: uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount       = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.emplace_back(queueCreateInfo);
		}

		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures{
		        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR
		};

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
		        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
		};

		accelFeature.pNext = &bufferDeviceAddressFeatures;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
		        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
		};

		rtPipelineFeature.pNext = &accelFeature;

		VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		features.pNext = &rtPipelineFeature;

		vkGetPhysicalDeviceFeatures2(physicalDevice, &features);

		VkDeviceCreateInfo createInfo{};
		createInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = queueCreateInfos.size();
		createInfo.pQueueCreateInfos    = queueCreateInfos.data();
		createInfo.pEnabledFeatures     = nullptr;
		createInfo.pNext                = &features;

		if (Instance::VALIDATION_LAYERS_ENABLED) {
			createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
			createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
		} else {
			createInfo.enabledLayerCount = 0;
		}

		createInfo.enabledExtensionCount   = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
		createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

		VkDevice device;
		if (const VkResult result{vkCreateDevice(physicalDevice, &createInfo, nullptr, &device)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create logical device: "} + string_VkResult(result)};
		}
		m_Device.reset(device);

		m_Swapchain.Create(this, window, surface, physicalDevice, m_DescSetLayout);

		VkWriteDescriptorSetAccelerationStructureKHR descASInfo{
		        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
		};
		descASInfo.accelerationStructureCount = 1;
		descASInfo.pAccelerationStructures    = &m_Tlas->m_hAccStruct;
		VkDescriptorImageInfo imageInfo{{}, m_Swapchain.GetOutputImageView(), VK_IMAGE_LAYOUT_GENERAL};

		// TODO: repeated code
		VkDescriptorBufferInfo dbiUnif{m_Swapchain.m_GlobalsBuffer.GetHandle(), 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet   globalWrite{
                MakeWrite(m_DescriptorSetLayoutBindings, m_DescSet, RtxBindings::eGlobals, &dbiUnif, 0)
        };
		std::array<VkWriteDescriptorSet, 4> writeDescriptorSets{globalWrite};
		writeDescriptorSets[1] =
		        MakeWrite(m_DescriptorSetLayoutBindings, m_DescSet, RtxBindings::eTlas, &descASInfo, 0);
		writeDescriptorSets[2] =
		        MakeWrite(m_DescriptorSetLayoutBindings, m_DescSet, RtxBindings::eOutImage, &imageInfo, 0);

		VkDescriptorBufferInfo dbiSceneDesc{m_ObjDescBuffer.GetHandle(), 0, VK_WHOLE_SIZE};
		writeDescriptorSets[3] =
		        MakeWrite(m_DescriptorSetLayoutBindings, m_DescSet, RtxBindings::eObjDescs, &dbiSceneDesc, 0);

		vkUpdateDescriptorSets(GetHandle(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
		//end of todo

		CreatePostDescriptorSet();
		CreatePostRenderPass();
		CreatePostPipeline();
		UpdatePostDescriptorSet();
		m_Swapchain.CreateFramebuffers();

		FillRtDescriptorSet();

		m_Swapchain.CreateRtShaderBindingTable(physicalDevice);
	}

	void Device::CopyBuffer(const CommandPool &commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	        const {
		CommandBuffer commandBuffer{commandPool.AllocateCommandBuffer()};

		commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size      = size;

		vkCmdCopyBuffer(commandBuffer.GetHandle(), srcBuffer, dstBuffer, 1, &copyRegion);

		commandBuffer.End();

		commandBuffer.SubmitAndWait(m_GraphicsQueue);
	}

	void Device::QueueSubmit(uint32_t submitCount, const VkSubmitInfo *pSubmitInfo, VkFence fence) {
		if (const VkResult result{vkQueueSubmit(m_GraphicsQueue, submitCount, pSubmitInfo, fence)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to submit draw command m_Buffer: "} + string_VkResult(result)};
		}
	}

	bool Device::QueuePresent(const VkPresentInfoKHR *pPresentInfo, Window &window) {
		if (const VkResult result{vkQueuePresentKHR(m_PresentQueue, pPresentInfo)}; result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.PollWasResized()) {
				return true;
			}

			throw std::runtime_error{std::string{"Failed to present swap chain image: "} + string_VkResult(result)};
		}

		return false;
	}

	void Device::RecreateSwapchain(const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice) {
		int width, height;
		glfwGetFramebufferSize(window.Get(), &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window.Get(), &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(GetHandle());

		m_Swapchain.Recreate(this, window, surface, physicalDevice);
	}

	Buffer Device::CreateBuffer(
	        VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
	        VkMemoryPropertyFlags properties
	) const {
		DEBUG("Creating m_Buffer of size " << size << "...");

		VkBuffer       buffer;
		VkDeviceMemory bufferMemory;

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size        = size;
		bufferInfo.usage       = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (const VkResult result{vkCreateBuffer(GetHandle(), &bufferInfo, nullptr, &buffer)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create m_Buffer: "} + string_VkResult(result)};
		}

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(GetHandle(), buffer, &memoryRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex =
		        FindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties);

		VkMemoryAllocateFlagsInfo flagsInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
		if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) > 0) {
			flagsInfo.flags          = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			memoryAllocateInfo.pNext = &flagsInfo;
		}

		DEBUG("trying to allocate memory");
		if (const VkResult result{vkAllocateMemory(GetHandle(), &memoryAllocateInfo, nullptr, &bufferMemory)};
		    result != VK_SUCCESS) {
			DEBUG("failed " << string_VkResult(result));
			throw std::runtime_error{std::string{"Failed to allocate memory: "} + string_VkResult(result)};
		}
		DEBUG("memory allocation succeeded");

		vkBindBufferMemory(GetHandle(), buffer, bufferMemory, 0);

		DEBUG("Buffer created");
		return Buffer{*this, buffer, bufferMemory};
	}

	uint32_t
	Device::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

		//TODO: prefer vram resource
		for (uint32_t i{0}; i < memoryProperties.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error{"Failed to find a suitable memory type"};
	}

	void Device::vkGetAccelerationStructureBuildSizesKHR(
	        VkDevice device, VkAccelerationStructureBuildTypeKHR buildType,
	        const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo, const uint32_t *pMaxPrimitiveCounts,
	        VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo
	) {
		const auto func{reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
		        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkGetAccelerationStructureBuildSizesKHR function"};
		}

		return func(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
	}

	void vkCmdCopyAccelerationStructureKHR(
	        VkDevice device, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR *copyInfo
	) {
		const auto func{reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
		        vkGetDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkCmdCopyAccelerationStructureKHR function"};
		}

		return func(commandBuffer, copyInfo);
	}

	VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(
	        VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR *addressInfo
	) {
		const auto func{reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
		        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkGetAccelerationStructureDeviceAddressKHR function"};
		}

		return func(device, addressInfo);
	}

	void Device::CmdCompactBlas(
	        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
	        std::vector<BuildAccelerationStructure> &buildAs, VkQueryPool queryPool
	) {
		uint32_t queryCount{0};

		std::vector<VkDeviceSize> compactSizes{indices.size()};
		vkGetQueryPoolResults(
		        GetHandle(), queryPool, 0, compactSizes.size(), compactSizes.size() * sizeof(VkDeviceSize),
		        compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT
		);

		for (auto idx: indices) {
			buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCount++];

			VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
			asCreateInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;
			asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

			buildAs[idx].accelerationStructure = CreateAccelerationStructure(physicalDevice, asCreateInfo);

			VkCopyAccelerationStructureInfoKHR copyInfo{};
			copyInfo.src  = buildAs[idx].buildInfo.dstAccelerationStructure;
			copyInfo.dst  = buildAs[idx].accelerationStructure.m_hAccStruct;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
			vkCmdCopyAccelerationStructureKHR(GetHandle(), cmdBuf, &copyInfo);
		}
	}

	void Device::CreateTopLevelAccelerationStructure(
	        VkPhysicalDevice physicalDevice, const std::vector<ModelInstance> &modelInstances
	) {
		std::vector<VkAccelerationStructureInstanceKHR> tlas{};
		tlas.reserve(modelInstances.size());

		for (const ModelInstance &modelInstance: modelInstances) {
			VkAccelerationStructureInstanceKHR rayInst{};
			rayInst.transform                              = utils::ToTransformMatrixKHR(modelInstance.m_ModelMatrix);
			rayInst.instanceCustomIndex                    = modelInstance.objIndex;
			rayInst.accelerationStructureReference         = GetBlasDeviceAddress(modelInstance.objIndex);
			rayInst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			rayInst.mask                                   = 0xFF;
			rayInst.instanceShaderBindingTableRecordOffset = 0;
			tlas.emplace_back(rayInst);
		}

		BuildTlas(physicalDevice, tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
	}

	void Device::SetUpRaytracing(
	        VkPhysicalDevice physicalDevice, const std::vector<Model> &models,
	        const std::vector<ModelInstance> &modelInstances
	) {
		CreateBottomLevelAccelerationStructure(physicalDevice, models);
		CreateTopLevelAccelerationStructure(physicalDevice, modelInstances);
	}

	VkDeviceAddress Device::GetBlasDeviceAddress(uint32_t blasId) const {
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR
		};
		addressInfo.accelerationStructure = m_Blas[blasId].accelerationStructure.m_hAccStruct;

		return roing::vk::vkGetAccelerationStructureDeviceAddressKHR(GetHandle(), &addressInfo);
	}

	void Device::UpdateRtDescriptorSet() {
		VkDescriptorImageInfo imageInfo{{}, m_Swapchain.GetOutputImageView(), VK_IMAGE_LAYOUT_GENERAL};
		VkWriteDescriptorSet  outImageWrite{MakeWrite(
                m_DescriptorSetLayoutBindings, m_DescSet, static_cast<uint32_t>(RtxBindings::eOutImage), &imageInfo, 0
        )};

		VkDescriptorBufferInfo dbiUnif{m_Swapchain.m_GlobalsBuffer.GetHandle(), 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet   globalWrite{MakeWrite(
                m_DescriptorSetLayoutBindings, m_DescSet, static_cast<uint32_t>(RtxBindings::eGlobals), &dbiUnif, 0
        )};
		//		VkDescriptorBufferInfo              dbiSceneDesc{m};
		std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{outImageWrite, globalWrite};

		vkUpdateDescriptorSets(GetHandle(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
	}

	VkWriteDescriptorSet Device::MakeWrite(
	        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
	        uint32_t arrayElement
	) {
		VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;

		for (const auto &binding: bindings) {
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

	VkWriteDescriptorSet Device::MakeWrite(
	        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
	        const VkWriteDescriptorSetAccelerationStructureKHR *pAccelerationStructure, uint32_t arrayElement
	) {
		VkWriteDescriptorSet descriptorSet{MakeWrite(bindings, dstSet, dstBinding, arrayElement)};
		descriptorSet.pNext = pAccelerationStructure;

		return descriptorSet;
	}

	VkWriteDescriptorSet Device::MakeWrite(
	        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
	        const VkDescriptorImageInfo *pImageInfo, uint32_t arrayElement
	) {
		VkWriteDescriptorSet descriptorSet{MakeWrite(bindings, dstSet, dstBinding, arrayElement)};
		descriptorSet.pImageInfo = pImageInfo;

		return descriptorSet;
	}

	VkWriteDescriptorSet Device::MakeWrite(
	        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
	        const VkDescriptorBufferInfo *pBufferInfo, uint32_t arrayElement
	) {
		VkWriteDescriptorSet descriptorSet{MakeWrite(bindings, dstSet, dstBinding, arrayElement)};
		descriptorSet.pBufferInfo = pBufferInfo;

		return descriptorSet;
	}

	void Device::FillRtDescriptorSet() {
		VkWriteDescriptorSetAccelerationStructureKHR descASInfo{
		        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
		};
		descASInfo.accelerationStructureCount = 1;
		descASInfo.pAccelerationStructures    = &m_Tlas->m_hAccStruct;

		VkDescriptorImageInfo imageInfo{{}, m_Swapchain.GetOutputImageView(), VK_IMAGE_LAYOUT_GENERAL};

		std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
		writeDescriptorSets.emplace_back(MakeWrite(
		        m_DescriptorSetLayoutBindings, m_DescSet, static_cast<uint32_t>(RtxBindings::eTlas), &descASInfo, 0
		));
		writeDescriptorSets.emplace_back(MakeWrite(
		        m_DescriptorSetLayoutBindings, m_DescSet, static_cast<uint32_t>(RtxBindings::eOutImage), &imageInfo, 0
		));
		vkUpdateDescriptorSets(
		        GetHandle(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr
		);
	}

	void Device::CreatePostDescriptorSet() {
		m_PostDescriptorSetLayoutBindings.emplace_back(
		        0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_PostDescriptorSetLayout =
		        CreateDescriptorSetLayout(0, m_PostDescriptorSetLayoutBindingFlags, m_PostDescriptorSetLayoutBindings);
		m_PostDescriptorPool = CreatePool();

		VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocateInfo.descriptorPool     = m_PostDescriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts        = &m_PostDescriptorSetLayout;
		vkAllocateDescriptorSets(GetHandle(), &allocateInfo, &m_PostDescriptorSet);
	}

	void Device::UpdatePostDescriptorSet() {
		VkWriteDescriptorSet writeDescriptorSet{MakeWrite(
		        m_PostDescriptorSetLayoutBindings, m_PostDescriptorSet, 0, &m_Swapchain.GetOutputTexture().descriptor, 0
		)};
		vkUpdateDescriptorSets(GetHandle(), 1, &writeDescriptorSet, 0, nullptr);
	}

	void Device::CreatePostPipeline() {
		VkPushConstantRange pushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)};

		VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		createInfo.setLayoutCount         = 1;
		createInfo.pSetLayouts            = &m_PostDescriptorSetLayout;
		createInfo.pushConstantRangeCount = 1;
		createInfo.pPushConstantRanges    = &pushConstantRange;
		if (const VkResult result{vkCreatePipelineLayout(GetHandle(), &createInfo, nullptr, &m_PostPipelineLayout)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create pipeline layout: "} + string_VkResult(result)};
		}

		auto vertShaderCode{utils::ReadFile("shaders/shader.vert.spv")};
		auto fragShaderCode{utils::ReadFile("shaders/shader.frag.spv")};

		VkShaderModule vertShaderModule{m_Swapchain.CreateShaderModule(std::move(vertShaderCode))};
		VkShaderModule fragShaderModule{m_Swapchain.CreateShaderModule(std::move(fragShaderCode))};

		VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		vertShaderStageCreateInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageCreateInfo.module = vertShaderModule;
		vertShaderStageCreateInfo.pName  = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		fragShaderStageCreateInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageCreateInfo.module = fragShaderModule;
		fragShaderStageCreateInfo.pName  = "main";

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
		        vertShaderStageCreateInfo, fragShaderStageCreateInfo
		};

		std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
		dynamicStateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = dynamicStates.size();
		dynamicStateInfo.pDynamicStates    = dynamicStates.data();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		vertexInputInfo.vertexBindingDescriptionCount   = 0;
		vertexInputInfo.pVertexBindingDescriptions      = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x        = 0.0f;
		viewport.y        = 0.0f;
		viewport.width    = static_cast<float>(m_Swapchain.GetExtent().width);
		viewport.height   = static_cast<float>(m_Swapchain.GetExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = m_Swapchain.GetExtent();

		VkPipelineViewportStateCreateInfo viewportStateInfo{};
		viewportStateInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.scissorCount  = 1;

		VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
		rasterizerInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerInfo.depthClampEnable        = VK_FALSE;
		rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerInfo.polygonMode             = VK_POLYGON_MODE_FILL;
		rasterizerInfo.lineWidth               = 1.0f;
		rasterizerInfo.cullMode                = VK_CULL_MODE_NONE;
		rasterizerInfo.frontFace               = VK_FRONT_FACE_CLOCKWISE;
		rasterizerInfo.depthBiasEnable         = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampleInfo{};
		multisampleInfo.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleInfo.sampleShadingEnable  = VK_FALSE;
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable         = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable     = VK_FALSE;
		colorBlending.logicOp           = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount   = 1;
		colorBlending.pAttachments      = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount          = shaderStages.size();
		pipelineCreateInfo.pStages             = shaderStages.data();
		pipelineCreateInfo.pVertexInputState   = &vertexInputInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineCreateInfo.pViewportState      = &viewportStateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
		pipelineCreateInfo.pMultisampleState   = &multisampleInfo;
		pipelineCreateInfo.pDepthStencilState  = nullptr;
		pipelineCreateInfo.pColorBlendState    = &colorBlending;
		pipelineCreateInfo.pDynamicState       = &dynamicStateInfo;
		pipelineCreateInfo.layout              = m_PostPipelineLayout;
		pipelineCreateInfo.renderPass          = m_RenderPass;
		pipelineCreateInfo.subpass             = 0;

		// Creating new pipeline from existing one!
		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex  = -1;

		if (const VkResult result{vkCreateGraphicsPipelines(
		            GetHandle(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_PostPipeline
		    )};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create graphics pipeline: "} + string_VkResult(result)};
		}

		vkDestroyShaderModule(GetHandle(), vertShaderModule, nullptr);
		vkDestroyShaderModule(GetHandle(), fragShaderModule, nullptr);
	}

	void Device::CreatePostRenderPass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format         = m_Swapchain.GetFormat();
		colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments    = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments    = &colorAttachment;
		renderPassCreateInfo.subpassCount    = 1;
		renderPassCreateInfo.pSubpasses      = &subpassDescription;

		VkSubpassDependency dependency{};
		dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass    = 0;
		dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies   = &dependency;

		if (const VkResult result{vkCreateRenderPass(GetHandle(), &renderPassCreateInfo, nullptr, &m_RenderPass)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create render pass: "} + string_VkResult(result)};
		}
	}
}// namespace roing::vk
