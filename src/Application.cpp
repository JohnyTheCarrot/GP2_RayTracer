#include "Application.h"
#include "src/vk/DescriptorPool.h"
#include "src/vk/PhysicalDeviceUtils.h"
#include "utils/ToTransformMatrixKHR.h"
#include "vk/Device.h"
#include "vk/HostDevice.h"
#include "vk/RayTracingExtFunctions.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing {
	void Application::Run() {
		InitVulkan();
		MainLoop();
	}

	void Application::InitVulkan() {
		//		InitRayTracing();
		//		m_Device.SetUpRaytracing(m_PhysicalDevice, m_Models, m_ModelInstances);
	}

	void Application::MainLoop() {
		while (!glfwWindowShouldClose(m_Window.Get())) {
			glfwPollEvents();

			DrawFrame();
		}

		vkDeviceWaitIdle(m_Device.GetHandle());
	}

	void Application::DrawFrame() {
		if (m_RenderProcess.DrawFrame(m_Device, m_Window)) {
			m_Device.UpdatePostDescriptorSet();
			m_Device.UpdateRtDescriptorSet();
			m_Device.RecreateSwapchain(m_Window, m_Surface, m_PhysicalDevice);
		}
	}

	std::vector<BuildAccelerationStructure> Application::CreateBottomLevelAccelerationStructure(
	        const vk::Device &device, VkPhysicalDevice physicalDevice, vk::CommandPool &commandPool,
	        VkQueue graphicsQueue, const std::vector<Model> &models
	) {
		std::vector<BlasInput> blasInputs{};
		blasInputs.reserve(models.size());

		std::transform(models.cbegin(), models.cend(), std::back_inserter(blasInputs), [](const Model &model) {
			return model.ObjectToVkGeometry();
		});

		BuildBlas(device, physicalDevice, commandPool, graphicsQueue, blasInputs);
	}

	std::vector<BuildAccelerationStructure> Application::BuildBlas(
	        const vk::Device &device, VkPhysicalDevice hPhysicalDevice, vk::CommandPool &commandPool,
	        VkQueue graphicsQueue, const std::vector<BlasInput> &input
	) {
		std::vector<BuildAccelerationStructure> buildAs{};
		VkDeviceSize                            asTotalSize{0};
		uint32_t                                nbCompactions{0};
		VkDeviceSize                            maxScratchSize{0};

		for (size_t idx{0}; idx < buildAs.size(); ++idx) {
			VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
			        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
			};

			buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildInfo.flags         = input[idx].flags | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			buildInfo.geometryCount = input[idx].accStructGeometry.size();
			buildInfo.pGeometries   = input[idx].accStructGeometry.data();

			const VkAccelerationStructureBuildRangeInfoKHR *rangeInfo{};
			rangeInfo = input[idx].accStructBuildOffsetInfo.data();

			std::vector<uint32_t> maxPrimCount(input[idx].accStructBuildOffsetInfo.size());
			for (size_t infoIdx{0}; infoIdx < maxPrimCount.size(); ++infoIdx) {
				maxPrimCount[infoIdx] = input[idx].accStructBuildOffsetInfo[infoIdx].primitiveCount;
			}

			VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
			        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
			};
			vkGetAccelerationStructureBuildSizesKHR(
			        device.GetHandle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
			        maxPrimCount.data(), &sizeInfo
			);

			asTotalSize += sizeInfo.accelerationStructureSize;
			maxScratchSize = std::max(maxScratchSize, sizeInfo.buildScratchSize);
			nbCompactions += (buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) > 0;

			buildAs.emplace_back(buildInfo, sizeInfo, rangeInfo, std::nullopt);
		}

		vk::Buffer      scratchBuffer{device.CreateBuffer(
                hPhysicalDevice, maxScratchSize,
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
			vkCreateQueryPool(device.GetHandle(), &queryPoolCreateInfo, nullptr, &queryPool);
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

			vk::CommandBuffer createBlasCommandBuffer{commandPool.AllocateCommandBuffer()};
			CmdCreateBlas(
			        device, hPhysicalDevice, createBlasCommandBuffer, indices, buildAs, scratchAddress, queryPool
			);
			createBlasCommandBuffer.SubmitAndWait(graphicsQueue);

			if (queryPool) {
				DEBUG("Blas compaction requested but not implemented!");
				//				VkCommandBuffer compactCmdBuf{m_Device.CreateCommandBuffer()};
				//				CmdCompactBlas(hPhysicalDevice, compactCmdBuf, indices, buildAs, queryPool);
				//				SubmitAndWait(compactCmdBuf);
				//
				//				for (auto index: indices) {
				//					roing::vk::vkDestroyAccelerationStructureKHR(
				//					        m_Device.GetHandle(), buildAs[index].accelerationStructure.m_hAccStruct, nullptr
				//					);
				//				}
			}

			batchSize = 0;
			indices.clear();
		}


		std::vector<BuildAccelerationStructure> blas(buildAs.size());
		std::move(buildAs.begin(), buildAs.end(), blas.begin());

		vkDestroyQueryPool(device.GetHandle(), queryPool, nullptr);

		return blas;
	}

	Application::Application(std::function<std::vector<Model>(const vk::Device &, const vk::CommandPool &, VkPhysicalDevice)> modelLoader, const std::function<std::vector<ModelInstance>(const std::vector<Model> &)> &instanceLoader)
	    : m_Window{WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE}
	    , m_Instance{}
	    , m_Surface{m_Instance, m_Window}
	    , m_PhysicalDevice{m_Instance.PickPhysicalDevice(m_Surface)}
	    , m_Device{
	              m_Surface,
	              m_PhysicalDevice,
	              m_Window,
	      }
 		, m_DescPool{CreatePool(m_Device)}
 		, m_DescSetLayout{m_Device, 0, {},  {VkDescriptorSetLayoutBinding{
	                                               static_cast<int>(RtxBindings::eTlas), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
	                                               VK_SHADER_STAGE_RAYGEN_BIT_KHR
	                                       },
	                      {static_cast<int>(RtxBindings::eOutImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
	                       VK_SHADER_STAGE_RAYGEN_BIT_KHR},
	                      {static_cast<uint32_t>(RtxBindings::eGlobals), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
	                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR},
	                      {static_cast<uint32_t>(RtxBindings::eObjDescs), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
	                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}}}
 		, m_DescSet{m_Device, m_DescPool, m_DescSetLayout}
	    , m_CommandPool{m_Device, m_Surface, m_PhysicalDevice}
 		, m_Models{modelLoader(m_Device, m_CommandPool, m_PhysicalDevice)}
 		, m_ModelInstances{instanceLoader(m_Models)}
		, m_ObjDescriptors{CreateObjDescs(m_Models)}
 		, m_ObjDescBuffer{m_Device.CreateBuffer(m_PhysicalDevice, sizeof(ObjDesc) * m_ObjDescriptors.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
		, m_QueueFamilyIndices{vk::physical_device::FindQueueFamilies(m_Surface, m_PhysicalDevice)}
 		, m_GraphicsQueue{GetDeviceQueue(m_Device, m_QueueFamilyIndices.graphicsFamily.value())}
 		, m_PresentQueue{GetDeviceQueue(m_Device, m_QueueFamilyIndices.presentFamily.value())}
		, m_Blas{CreateBottomLevelAccelerationStructure(m_Device, m_PhysicalDevice, m_CommandPool, m_GraphicsQueue, m_Models)}
 		, m_Tlas{CreateTopLevelAccelerationStructure(m_Device, m_PhysicalDevice, m_CommandPool, m_GraphicsQueue, m_Blas, m_ModelInstances)}
 		// TODO: swapchian
	    // TODO: m_SwapChainImageViews
	    , m_RtPipeline{m_Device, m_DescSetLayout.GetHandle()}
	    , m_GlobalsBuffer{m_Device.CreateBuffer(m_PhysicalDevice, sizeof(GlobalUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)}
	{
		VkDeviceSize objDescBufferSize{sizeof(ObjDesc) * m_ObjDescriptors.size()};

		// TODO: can we abstract this mapping and void* stuff away?
		void *data;
		DEBUG("objDescBuffer mem handle: " << m_ObjDescBuffer.GetMemoryHandle());
		vkMapMemory(m_Device.GetHandle(), m_ObjDescBuffer.GetMemoryHandle(), 0, objDescBufferSize, 0, &data);
		DEBUG("data: " << std::hex << data << std::dec);
		memcpy(data, m_ObjDescriptors.data(), objDescBufferSize);
		vkUnmapMemory(m_Device.GetHandle(), m_ObjDescBuffer.GetMemoryHandle());
	}

	void Application::CmdCreateBlas(
	        const vk::Device &device, VkPhysicalDevice hPhysicalDevice, vk::CommandBuffer &commandBuffer,
	        const std::vector<uint32_t> &indices, std::vector<BuildAccelerationStructure> &buildAs,
	        VkDeviceAddress scratchAddress, VkQueryPool queryPool
	) {
		if (queryPool != VK_NULL_HANDLE) {
			vkResetQueryPool(device.GetHandle(), queryPool, 0, indices.size());
		}
		uint32_t queryCount{0};

		for (auto idx: indices) {
			VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;

			buildAs[idx].accelerationStructure = vk::AccelerationStructure{device, hPhysicalDevice, createInfo};

			buildAs[idx].buildInfo.dstAccelerationStructure =
			        buildAs[idx].accelerationStructure.value().m_hAccStruct.get();
			buildAs[idx].buildInfo.scratchData.deviceAddress = scratchAddress;

			roing::vk::vkCmdBuildAccelerationStructuresKHR(
			        device.GetHandle(), commandBuffer.GetHandle(), 1, &buildAs[idx].buildInfo, &buildAs[idx].rangeInfo
			);

			// Since the scratch m_Buffer is reused across builds, we need a barrier to ensure one build
			// is finished before starting the next one.
			VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

			vkCmdPipelineBarrier(
			        commandBuffer.GetHandle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr
			);

			if (queryPool != VK_NULL_HANDLE) {
				roing::vk::vkCmdWriteAccelerationStructuresPropertiesKHR(
				        device.GetHandle(), commandBuffer.GetHandle(), 1,
				        &buildAs[idx].buildInfo.dstAccelerationStructure,
				        VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCount
				);
				++queryCount;
			}
		}
	}

	vk::AccelerationStructure Application::BuildTlas(
	        const vk::Device &device, const vk::CommandPool &commandPool, VkPhysicalDevice hPhysicalDevice,
	        VkQueue graphicsQueue, const std::vector<VkAccelerationStructureInstanceKHR> &instances,
	        VkBuildAccelerationStructureFlagsKHR flags
	) {
		uint32_t countInstance{static_cast<uint32_t>(instances.size())};

		vk::CommandBuffer cmdBuf{commandPool.AllocateCommandBuffer()};

		VkDeviceSize instancesBufferSize{sizeof(VkAccelerationStructureInstanceKHR) * instances.size()};
		vk::Buffer   instancesBuffer{device.CreateBuffer(
                hPhysicalDevice, instancesBufferSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )};

		VkDeviceAddress instBufferAddr{instancesBuffer.GetDeviceAddress()};

		void *data;
		vkMapMemory(device.GetHandle(), instancesBuffer.GetMemoryHandle(), 0, instancesBufferSize, 0, &data);

		memcpy(data, instances.data(), instancesBufferSize);

		vkUnmapMemory(device.GetHandle(), instancesBuffer.GetMemoryHandle());

		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(
		        cmdBuf.GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT,
		        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr
		);

		vk::Buffer scratchBuffer;
		CmdCreateTlas(device, hPhysicalDevice, cmdBuf, countInstance, instBufferAddr, scratchBuffer, flags);

		cmdBuf.SubmitAndWait(graphicsQueue);
	}

	VkQueue Application::GetDeviceQueue(const vk::Device &device, uint32_t familyIndex) {
		VkQueue queue;
		vkGetDeviceQueue(device.GetHandle(), familyIndex, 0, &queue);
		return queue;
	}

	vk::AccelerationStructure Application::CmdCreateTlas(
	        const vk::Device &device, VkPhysicalDevice physicalDevice, vk::CommandBuffer &cmdBuf,
	        uint32_t countInstance, VkDeviceAddress instBufferAddr, vk::Buffer &scratchBuffer,
	        VkBuildAccelerationStructureFlagsKHR flags
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
		buildGeometryInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildGeometryInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
		};
		vkGetAccelerationStructureBuildSizesKHR(
		        device.GetHandle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &countInstance,
		        &sizeInfo
		);

		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		createInfo.size = sizeInfo.accelerationStructureSize;

		vk::AccelerationStructure tlas{device, physicalDevice, createInfo};
		scratchBuffer = device.CreateBuffer(
		        physicalDevice, sizeInfo.buildScratchSize,
		        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		);
		VkDeviceAddress scratchBufferAddr{scratchBuffer.GetDeviceAddress()};

		buildGeometryInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
		buildGeometryInfo.dstAccelerationStructure  = tlas.m_hAccStruct.get();
		buildGeometryInfo.scratchData.deviceAddress = scratchBufferAddr;

		VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{countInstance, 0, 0, 0};
		const VkAccelerationStructureBuildRangeInfoKHR *pBuildOffsetInfo{&buildOffsetInfo};

		roing::vk::vkCmdBuildAccelerationStructuresKHR(
		        device.GetHandle(), cmdBuf.GetHandle(), 1, &buildGeometryInfo, &pBuildOffsetInfo
		);
	}

	vk::AccelerationStructure Application::CreateTopLevelAccelerationStructure(
	        const vk::Device &device, VkPhysicalDevice physicalDevice, const vk::CommandPool &commandPool,
	        VkQueue graphicsQueue, const std::vector<BuildAccelerationStructure> &blas,
	        const std::vector<ModelInstance> &modelInstances
	) {
		std::vector<VkAccelerationStructureInstanceKHR> tlasInstances{};
		tlasInstances.reserve(modelInstances.size());

		for (const ModelInstance &modelInstance: modelInstances) {
			VkAccelerationStructureInstanceKHR rayInst{};
			rayInst.transform                              = utils::ToTransformMatrixKHR(modelInstance.m_ModelMatrix);
			rayInst.instanceCustomIndex                    = modelInstance.objIndex;
			rayInst.accelerationStructureReference         = GetBlasDeviceAddress(device, blas, modelInstance.objIndex);
			rayInst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			rayInst.mask                                   = 0xFF;
			rayInst.instanceShaderBindingTableRecordOffset = 0;
			tlasInstances.emplace_back(rayInst);
		}

		return BuildTlas(
		        device, commandPool, physicalDevice, graphicsQueue, tlasInstances,
		        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
		);
	}

	VkDeviceAddress Application::GetBlasDeviceAddress(
	        const vk::Device &device, const std::vector<BuildAccelerationStructure> &blas, uint32_t blasId
	) {
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR
		};
		addressInfo.accelerationStructure = blas[blasId].accelerationStructure.value().m_hAccStruct.get();

		return roing::vk::vkGetAccelerationStructureDeviceAddressKHR(device.GetHandle(), &addressInfo);
	}

	std::vector<ObjDesc> Application::CreateObjDescs(const std::vector<Model> &models) {
		std::vector<ObjDesc> objDescs(models.size());
		std::transform(models.begin(), models.end(), objDescs.begin(), [](const Model &model) {
			ObjDesc desc{};
			desc.txtOffset     = 0;
			desc.vertexAddress = model.GetVertexBufferAddress();
			desc.indexAddress  = model.GetIndexBufferAddress();
			return desc;
		});
		return objDescs;
	}

	vk::DescriptorPool
	Application::CreatePool(const vk::Device &device, uint32_t maxSets, VkDescriptorPoolCreateFlags flags) {
		std::vector<VkDescriptorPoolSize> poolSizes{};
		AddRequiredPoolSizes(poolSizes, maxSets);

		VkDescriptorPoolCreateInfo descPoolInfo{};
		descPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolInfo.pNext         = nullptr;
		descPoolInfo.maxSets       = maxSets;
		descPoolInfo.poolSizeCount = poolSizes.size();
		descPoolInfo.pPoolSizes    = poolSizes.data();
		descPoolInfo.flags         = flags;

		return vk::DescriptorPool{device, descPoolInfo};
	}

	void Application::AddRequiredPoolSizes(std::vector<VkDescriptorPoolSize> &poolSizes, uint32_t numSets) {
		for (const auto &binding: DESCRIPTOR_SET_LAYOUT_BINDINGS) {
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
}// namespace roing
