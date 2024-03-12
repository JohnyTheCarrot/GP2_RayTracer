#include "RayTracingExtFunctions.h"
#include <stdexcept>

namespace roing::vk {
	VkResult vkCreateRayTracingPipelinesKHR(
	        VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache,
	        uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
	        const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines
	) {
		const auto func{reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
		        vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkCreateRayTracingPipelinesKHR function"};
		}

		return func(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
	}

	VkResult vkGetRayTracingShaderGroupHandlesKHR(
	        VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData
	) {
		const auto func{reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
		        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR")
		)};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkGetRayTracingShaderGroupHandlesKHR function"};
		}

		return func(device, pipeline, firstGroup, groupCount, dataSize, pData);
	}

	void vkCmdTraceRaysKHR(
	        VkDevice device, VkCommandBuffer commandBuffer,
	        const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable,
	        const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
	        const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable,
	        const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, uint32_t width, uint32_t height,
	        uint32_t depth
	) {
		const auto func{reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"))};

		if (func == nullptr) {
			throw std::runtime_error{"Failed to get vkCmdTraceRaysKHR function"};
		}

		return func(
		        commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable,
		        pCallableShaderBindingTable, width, height, depth
		);
	}

	void vkCmdBuildAccelerationStructuresKHR(
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

	void vkCmdWriteAccelerationStructuresPropertiesKHR(
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

	VkResult vkCreateAccelerationStructureKHR(
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
}// namespace roing::vk
