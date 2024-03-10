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
		if (m_Device == VK_NULL_HANDLE)
			return;

		DEBUG("Destroying device...");
		vkDestroyDescriptorPool(m_Device, m_DescPool, nullptr);
		vkDestroyDescriptorSetLayout(m_Device, m_DescSetLayout, nullptr);
		m_Models.clear();
		m_ObjDescBuffer.Destroy();
		m_ModelInstances.clear();
		m_Swapchain.Cleanup();
		m_Blas.clear();
		m_Tlas = std::nullopt;
		m_AccelerationStructureBuffers.clear();

		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

		vkDestroyDevice(m_Device, nullptr);
		DEBUG("Device destroyed");
	}

	Device::Device(
	        const Surface &surface, VkPhysicalDevice physicalDevice, const Window &window,
	        const std::vector<ModelLoadData> &models
	) {
		m_QueueFamilyIndices = physical_device::FindQueueFamilies(surface, physicalDevice);

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

		if (const VkResult result{vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_Device)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create logical device: "} + string_VkResult(result)};
		}

		vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.graphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.presentFamily.value(), 0, &m_PresentQueue);

		CreateCommandPool(surface, physicalDevice);

		for (const auto &modelLoadData: models) {
			const Model &model{m_Models.emplace_back(LoadModel(physicalDevice, modelLoadData.fileName))};
			m_ModelInstances.emplace_back(&model, modelLoadData.transform, static_cast<uint32_t>(m_Models.size() - 1));
			ObjDesc desc{};
			desc.txtOffset     = 0;
			desc.vertexAddress = model.GetVertexBufferAddress();
			desc.indexAddress  = model.GetIndexBufferAddress();

			m_ObjDescriptors.emplace_back(desc);
		}
		SetUpRaytracing(physicalDevice, m_Models, m_ModelInstances);

		CreateRtDescriptorSet();
		VkDeviceSize objDescBufferSize{sizeof(ObjDesc) * m_ObjDescriptors.size()};
		m_ObjDescBuffer = CreateBuffer(
		        physicalDevice, objDescBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		void *data;
		std::cout << "objDescBuffer mem handle: " << m_ObjDescBuffer.GetMemoryHandle() << std::endl;
		vkMapMemory(m_Device, m_ObjDescBuffer.GetMemoryHandle(), 0, objDescBufferSize, 0, &data);
		std::cout << "data: " << std::hex << data << std::dec << std::endl;
		memcpy(data, m_ObjDescriptors.data(), objDescBufferSize);
		vkUnmapMemory(m_Device, m_ObjDescBuffer.GetMemoryHandle());

		m_Swapchain.Create(this, window, surface, physicalDevice, m_DescSetLayout);

		VkWriteDescriptorSetAccelerationStructureKHR descASInfo{
		        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
		};
		descASInfo.accelerationStructureCount = 1;
		descASInfo.pAccelerationStructures    = &m_Tlas->accStructHandle;
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

		vkUpdateDescriptorSets(m_Device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
		//end of todo

		CreatePostDescriptorSet();
		CreatePostRenderPass();
		CreatePostPipeline();
		UpdatePostDescriptorSet();
		m_Swapchain.CreateFramebuffers();

		FillRtDescriptorSet();

		m_Swapchain.CreateRtShaderBindingTable(physicalDevice);
	}

	void Device::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool        = m_CommandPool;
		allocateInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(m_Device, &allocateInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size      = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers    = &commandBuffer;

		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_GraphicsQueue);

		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
	}

	void Device::QueueSubmit(uint32_t submitCount, const VkSubmitInfo *pSubmitInfo, VkFence fence) {
		if (const VkResult result{vkQueueSubmit(m_GraphicsQueue, submitCount, pSubmitInfo, fence)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to submit draw command buffer: "} + string_VkResult(result)};
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

	void Device::CreateCommandPool(const Surface &surface, VkPhysicalDevice physicalDevice) {
		QueueFamilyIndices queueFamilyIndices{vk::physical_device::FindQueueFamilies(surface, physicalDevice)};

		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (const VkResult result{vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create command pool: "} + string_VkResult(result)};
		}
	}

	void Device::RecreateSwapchain(const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice) {
		int width, height;
		glfwGetFramebufferSize(window.Get(), &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window.Get(), &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(m_Device);

		m_Swapchain.Recreate(this, window, surface, physicalDevice);
	}

	Buffer Device::CreateBuffer(
	        VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
	        VkMemoryPropertyFlags properties
	) const {
		DEBUG("Creating buffer of size " << size << "...");

		VkBuffer       buffer;
		VkDeviceMemory bufferMemory;

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size        = size;
		bufferInfo.usage       = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (const VkResult result{vkCreateBuffer(GetHandle(), &bufferInfo, nullptr, &buffer)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create buffer: "} + string_VkResult(result)};
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

		std::cout << "trying" << std::endl;
		if (const VkResult result{vkAllocateMemory(GetHandle(), &memoryAllocateInfo, nullptr, &bufferMemory)};
		    result != VK_SUCCESS) {
			std::cout << "failed " << string_VkResult(result) << std::endl;
			throw std::runtime_error{std::string{"Failed to allocate memory: "} + string_VkResult(result)};
		}
		std::cout << "succeeded" << std::endl;

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

	void Device::BuildBlas(
	        VkPhysicalDevice physicalDevice, const std::vector<BlasInput> &input,
	        VkBuildAccelerationStructureFlagsKHR flags, std::vector<BuildAccelerationStructure> &blasOut
	) {
		std::vector<BuildAccelerationStructure> buildAs(input.size());
		VkDeviceSize                            asTotalSize{0};
		uint32_t                                nbCompactions{0};
		VkDeviceSize                            maxScratchSize{0};

		for (size_t idx{0}; idx < buildAs.size(); ++idx) {
			BuildAccelerationStructure &build{buildAs[idx]};

			build.buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			build.buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			build.buildInfo.flags         = input[idx].flags | flags;
			build.buildInfo.geometryCount = input[idx].accStructGeometry.size();
			build.buildInfo.pGeometries   = input[idx].accStructGeometry.data();

			build.rangeInfo = input[idx].accStructBuildOffsetInfo.data();

			std::vector<uint32_t> maxPrimCount(input[idx].accStructBuildOffsetInfo.size());
			for (size_t infoIdx{0}; infoIdx < maxPrimCount.size(); ++infoIdx) {
				maxPrimCount[infoIdx] = input[idx].accStructBuildOffsetInfo[infoIdx].primitiveCount;
			}
			vkGetAccelerationStructureBuildSizesKHR(
			        m_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build.buildInfo, maxPrimCount.data(),
			        &build.sizeInfo
			);

			asTotalSize += build.sizeInfo.accelerationStructureSize;
			maxScratchSize = std::max(maxScratchSize, build.sizeInfo.buildScratchSize);
			nbCompactions += (build.buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) > 0;
		}

		Buffer          scratchBuffer{CreateBuffer(
                physicalDevice, maxScratchSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )};
		VkDeviceAddress scratchAddress{scratchBuffer.GetDeviceAddress()};

		VkQueryPool queryPool{VK_NULL_HANDLE};
		if (nbCompactions > 0) {
			assert(nbCompactions == input.size());// Don't allow mix of on/off compaction
			VkQueryPoolCreateInfo queryPoolCreateInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
			queryPoolCreateInfo.queryCount = input.size();
			queryPoolCreateInfo.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
			vkCreateQueryPool(m_Device, &queryPoolCreateInfo, nullptr, &queryPool);
		}

		std::vector<uint32_t>  indices{};
		VkDeviceSize           batchSize{0};
		constexpr VkDeviceSize batchLimit{256'000'000};
		for (uint32_t idx = 0; idx < input.size(); ++idx) {
			indices.push_back(idx);
			batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;

			if (batchSize < batchLimit && idx < input.size() - 1) {
				continue;
			}

			VkCommandBuffer createBlasCmdBuf{CreateCommandBuffer()};
			CmdCreateBlas(physicalDevice, createBlasCmdBuf, indices, buildAs, scratchAddress, queryPool);
			SubmitAndWait(createBlasCmdBuf);

			if (queryPool) {
				VkCommandBuffer compactCmdBuf{CreateCommandBuffer()};
				CmdCompactBlas(physicalDevice, compactCmdBuf, indices, buildAs, queryPool);
				SubmitAndWait(compactCmdBuf);

				for (auto index: indices) {
					roing::vk::vkDestroyAccelerationStructureKHR(
					        m_Device, buildAs[index].accelerationStructure.accStructHandle, nullptr
					);
				}
			}

			batchSize = 0;
			indices.clear();
		}

		blasOut.resize(buildAs.size());
		std::move(buildAs.begin(), buildAs.end(), blasOut.begin());

		vkDestroyQueryPool(m_Device, queryPool, nullptr);
	}

	void Device::BuildTlas(
	        VkPhysicalDevice physicalDevice, const std::vector<VkAccelerationStructureInstanceKHR> &instances,
	        VkBuildAccelerationStructureFlagsKHR flags, bool update
	) {
		assert(!m_Tlas.has_value() || m_Tlas->accStructHandle == VK_NULL_HANDLE || update);
		uint32_t countInstance{static_cast<uint32_t>(instances.size())};

		VkCommandBuffer cmdBuf{CreateCommandBuffer()};

		VkDeviceSize instancesBufferSize{sizeof(VkAccelerationStructureInstanceKHR) * instances.size()};
		Buffer       instancesBuffer{CreateBuffer(
                physicalDevice, instancesBufferSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )};

		VkDeviceAddress instBufferAddr{instancesBuffer.GetDeviceAddress()};

		void *data;
		vkMapMemory(m_Device, instancesBuffer.GetMemoryHandle(), 0, instancesBufferSize, 0, &data);

		memcpy(data, instances.data(), instancesBufferSize);

		vkUnmapMemory(m_Device, instancesBuffer.GetMemoryHandle());

		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(
		        cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
		        &barrier, 0, nullptr, 0, nullptr
		);

		Buffer scratchBuffer;
		CmdCreateTlas(physicalDevice, cmdBuf, countInstance, instBufferAddr, scratchBuffer, flags, update, false);

		SubmitAndWait(cmdBuf);
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

	void vkDestroyAccelerationStructureKHR(
	        VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks *pAllocator
	) {
		const auto func{reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
		        vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkDestroyAccelerationStructureKHR function"};
		}

		return func(device, accelerationStructure, pAllocator);
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

	VkResult Device::vkCreateAccelerationStructureKHR(
	        VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
	        const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure
	) {
		const auto func{reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
		        vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkCreateAccelerationStructureKHR function"};
		}

		return func(device, pCreateInfo, pAllocator, pAccelerationStructure);
	}

	void Device::vkCmdBuildAccelerationStructuresKHR(
	        VkDevice device, VkCommandBuffer commandBuffer, uint32_t infoCount,
	        const VkAccelerationStructureBuildGeometryInfoKHR     *pInfos,
	        const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos
	) {
		const auto func{reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
		        vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkCmdBuildAccelerationStructuresKHR function"};
		}

		return func(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
	}

	void Device::vkCmdWriteAccelerationStructuresPropertiesKHR(
	        VkDevice device, VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount,
	        const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool,
	        uint32_t firstQuery
	) {
		const auto func{reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
		        vkGetDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkCmdWriteAccelerationStructuresPropertiesKHR function"};
		}

		return func(
		        commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery
		);
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

	VkCommandBuffer Device::CreateCommandBuffer() {
		VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool                 = m_CommandPool;
		allocInfo.commandBufferCount          = 1;

		VkCommandBuffer cmd;
		vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo         = nullptr;

		vkBeginCommandBuffer(cmd, &beginInfo);

		return cmd;
	}

	void Device::SubmitAndWait(VkCommandBuffer commandBuffer) {
		Submit(commandBuffer);
		const VkResult result{vkQueueWaitIdle(m_GraphicsQueue)};

		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to submit command buffer command buffer commands");
		}
	}

	void Device::Submit(VkCommandBuffer commandBuffer) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submit       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submit.pCommandBuffers    = &commandBuffer;
		submit.commandBufferCount = 1;
		vkQueueSubmit(m_GraphicsQueue, 1, &submit, VK_NULL_HANDLE);
	}

	void Device::CmdCreateBlas(
	        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, const std::vector<uint32_t> &indices,
	        std::vector<BuildAccelerationStructure> &buildAs, VkDeviceAddress scratchAddress, VkQueryPool queryPool
	) {
		if (queryPool != VK_NULL_HANDLE) {
			vkResetQueryPool(m_Device, queryPool, 0, indices.size());
		}
		uint32_t queryCount{0};

		for (auto idx: indices) {
			VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};

			createInfo.type                    = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			createInfo.size                    = buildAs[idx].sizeInfo.accelerationStructureSize;
			buildAs[idx].accelerationStructure = CreateAccelerationStructure(physicalDevice, createInfo);

			buildAs[idx].buildInfo.dstAccelerationStructure  = buildAs[idx].accelerationStructure.accStructHandle;
			buildAs[idx].buildInfo.scratchData.deviceAddress = scratchAddress;

			vkCmdBuildAccelerationStructuresKHR(m_Device, cmdBuf, 1, &buildAs[idx].buildInfo, &buildAs[idx].rangeInfo);

			// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
			// is finished before starting the next one.
			VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

			vkCmdPipelineBarrier(
			        cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr
			);

			if (queryPool != VK_NULL_HANDLE) {
				vkCmdWriteAccelerationStructuresPropertiesKHR(
				        m_Device, cmdBuf, 1, &buildAs[idx].buildInfo.dstAccelerationStructure,
				        VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCount
				);
				++queryCount;
			}
		}
	}

	AccelerationStructure Device::CreateAccelerationStructure(
	        VkPhysicalDevice physicalDevice, VkAccelerationStructureCreateInfoKHR &createInfo
	) {
		AccelerationStructure accelerationStructure{};
		accelerationStructure.deviceHandle = m_Device;

		accelerationStructure.buffer = CreateBuffer(
		        physicalDevice, createInfo.size,
		        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		);

		createInfo.buffer = accelerationStructure.buffer.GetHandle();

		vkCreateAccelerationStructureKHR(m_Device, &createInfo, nullptr, &accelerationStructure.accStructHandle);

		return accelerationStructure;
	}

	void Device::CmdCompactBlas(
	        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
	        std::vector<BuildAccelerationStructure> &buildAs, VkQueryPool queryPool
	) {
		uint32_t queryCount{0};

		std::vector<VkDeviceSize> compactSizes{indices.size()};
		vkGetQueryPoolResults(
		        m_Device, queryPool, 0, compactSizes.size(), compactSizes.size() * sizeof(VkDeviceSize),
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
			copyInfo.dst  = buildAs[idx].accelerationStructure.accStructHandle;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
			vkCmdCopyAccelerationStructureKHR(m_Device, cmdBuf, &copyInfo);
		}
	}

	void
	Device::CreateBottomLevelAccelerationStructure(VkPhysicalDevice physicalDevice, const std::vector<Model> &models) {
		std::vector<BlasInput> blasInputs{};

		for (const auto &model: models) { blasInputs.emplace_back(model.ObjectToVkGeometry()); }

		BuildBlas(physicalDevice, blasInputs, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, m_Blas);
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
		addressInfo.accelerationStructure = m_Blas[blasId].accelerationStructure.accStructHandle;

		return roing::vk::vkGetAccelerationStructureDeviceAddressKHR(m_Device, &addressInfo);
	}

	void Device::CmdCreateTlas(
	        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, uint32_t countInstance,
	        VkDeviceAddress instBufferAddr, Buffer &scratchBuffer, VkBuildAccelerationStructureFlagsKHR flags,
	        bool update, bool motion
	) {
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR
		};
		instancesVk.data.deviceAddress = instBufferAddr;

		VkAccelerationStructureGeometryKHR topAccelerationStructureGeometry{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
		};
		topAccelerationStructureGeometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topAccelerationStructureGeometry.geometry.instances = instancesVk;

		VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
		};
		buildGeometryInfo.flags                    = flags;
		buildGeometryInfo.geometryCount            = 1;
		buildGeometryInfo.pGeometries              = &topAccelerationStructureGeometry;
		buildGeometryInfo.mode                     = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
		                                                    : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildGeometryInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
		};
		vkGetAccelerationStructureBuildSizesKHR(
		        m_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &countInstance, &sizeInfo
		);

		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		createInfo.size = sizeInfo.accelerationStructureSize;

		m_Tlas        = CreateAccelerationStructure(physicalDevice, createInfo);
		scratchBuffer = CreateBuffer(
		        physicalDevice, sizeInfo.buildScratchSize,
		        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		);
		VkDeviceAddress scratchBufferAddr{scratchBuffer.GetDeviceAddress()};

		buildGeometryInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
		buildGeometryInfo.dstAccelerationStructure  = m_Tlas->accStructHandle;
		buildGeometryInfo.scratchData.deviceAddress = scratchBufferAddr;

		VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{countInstance, 0, 0, 0};
		const VkAccelerationStructureBuildRangeInfoKHR *pBuildOffsetInfo{&buildOffsetInfo};

		vkCmdBuildAccelerationStructuresKHR(m_Device, cmdBuf, 1, &buildGeometryInfo, &pBuildOffsetInfo);
	}

	void Device::CreateRtDescriptorSet() {
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<int>(RtxBindings::eTlas), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
		        VK_SHADER_STAGE_RAYGEN_BIT_KHR
		);
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<int>(RtxBindings::eOutImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
		        VK_SHADER_STAGE_RAYGEN_BIT_KHR
		);

		// Camera matrices
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<uint32_t>(RtxBindings::eGlobals), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
		        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR
		);

		// Obj descriptions
		m_DescriptorSetLayoutBindings.emplace_back(
		        static_cast<uint32_t>(RtxBindings::eObjDescs), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
		        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
		);

		// TODO: Textures

		m_DescPool      = CreatePool();
		m_DescSetLayout = CreateDescriptorSetLayout(0, m_DescriptorBindingFlags, m_DescriptorSetLayoutBindings);

		VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocateInfo.descriptorPool     = m_DescPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts        = &m_DescSetLayout;
		vkAllocateDescriptorSets(m_Device, &allocateInfo, &m_DescSet);
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

		vkUpdateDescriptorSets(m_Device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
	}

	VkDescriptorPool Device::CreatePool(uint32_t maxSets, VkDescriptorPoolCreateFlags flags) {
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

		VkResult result{vkCreateDescriptorPool(m_Device, &descPoolInfo, nullptr, &descriptorPool)};
		assert(result == VK_SUCCESS);
		return descriptorPool;
	}

	void Device::AddRequiredPoolSizes(std::vector<VkDescriptorPoolSize> &poolSizes, uint32_t numSets) {
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

	VkDescriptorSetLayout Device::CreateDescriptorSetLayout(
	        VkDescriptorSetLayoutCreateFlags flags, std::vector<VkDescriptorBindingFlags> &bindingFlags,
	        std::vector<VkDescriptorSetLayoutBinding> &bindings
	) {
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingsInfo{
		        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO
		};

		if (bindingFlags.empty() && bindingFlags.size() < bindings.size()) {
			bindingFlags.resize(bindings.size(), 0);
		}

		bindingsInfo.bindingCount  = static_cast<uint32_t>(bindings.size());
		bindingsInfo.pBindingFlags = bindingFlags.data();

		VkDescriptorSetLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings    = bindings.data();
		createInfo.flags        = flags;
		createInfo.pNext        = bindingFlags.empty() ? nullptr : &bindingsInfo;

		VkDescriptorSetLayout descriptorSetLayout;
		vkCreateDescriptorSetLayout(m_Device, &createInfo, nullptr, &descriptorSetLayout);
		return descriptorSetLayout;
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

	bool Device::DrawFrame(Window &window) {
		return m_Swapchain.DrawFrame(*this, window, m_Models);
	}

	Model Device::LoadModel(VkPhysicalDevice physicalDevice, const std::string &fileName) {
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
		const auto &materials{reader.GetMaterials()};

		std::vector<Vertex>   modelVertices{};
		std::vector<uint32_t> modelIndices{};

		//		const auto &shape{shapes[0]};
		//		for (auto item: shape.mesh.indices) { modelIndices.push_back(item.vertex_index); }
		//		for (int idx{0}; idx < attrib.vertices.size(); idx += 3) {
		//			const float vx{static_cast<float>(attrib.vertices[idx + 0])};
		//			const float vy{static_cast<float>(attrib.vertices[idx + 1])};
		//			const float vz{static_cast<float>(attrib.vertices[idx + 2])};
		//
		//			modelVertices.emplace_back(glm::vec3{vx, vy, vz});
		//		}

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

		return Model{physicalDevice, *this, modelVertices, modelIndices};
	}

	void Device::FillRtDescriptorSet() {
		VkWriteDescriptorSetAccelerationStructureKHR descASInfo{
		        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
		};
		descASInfo.accelerationStructureCount = 1;
		descASInfo.pAccelerationStructures    = &m_Tlas->accStructHandle;

		VkDescriptorImageInfo imageInfo{{}, m_Swapchain.GetOutputImageView(), VK_IMAGE_LAYOUT_GENERAL};

		std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
		writeDescriptorSets.emplace_back(MakeWrite(
		        m_DescriptorSetLayoutBindings, m_DescSet, static_cast<uint32_t>(RtxBindings::eTlas), &descASInfo, 0
		));
		writeDescriptorSets.emplace_back(MakeWrite(
		        m_DescriptorSetLayoutBindings, m_DescSet, static_cast<uint32_t>(RtxBindings::eOutImage), &imageInfo, 0
		));
		vkUpdateDescriptorSets(
		        m_Device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr
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
		vkAllocateDescriptorSets(m_Device, &allocateInfo, &m_PostDescriptorSet);
	}

	void Device::UpdatePostDescriptorSet() {
		VkWriteDescriptorSet writeDescriptorSet{MakeWrite(
		        m_PostDescriptorSetLayoutBindings, m_PostDescriptorSet, 0, &m_Swapchain.GetOutputTexture().descriptor, 0
		)};
		vkUpdateDescriptorSets(m_Device, 1, &writeDescriptorSet, 0, nullptr);
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
