#ifndef PORTAL2RAYTRACED_RAYTRACINGEXTFUNCTIONS_H
#define PORTAL2RAYTRACED_RAYTRACINGEXTFUNCTIONS_H

#include <vulkan/vulkan.h>

namespace roing::vk {
	VkResult vkCreateRayTracingPipelinesKHR(
	        VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache,
	        uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
	        const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines
	);

	VkResult vkGetRayTracingShaderGroupHandlesKHR(
	        VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData
	);

	void vkCmdTraceRaysKHR(
	        VkDevice device, VkCommandBuffer commandBuffer,
	        const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable,
	        const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
	        const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable,
	        const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, uint32_t width, uint32_t height,
	        uint32_t depth
	);
}// namespace roing::vk

#endif//PORTAL2RAYTRACED_RAYTRACINGEXTFUNCTIONS_H
