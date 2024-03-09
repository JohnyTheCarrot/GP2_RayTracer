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
}// namespace roing::vk
