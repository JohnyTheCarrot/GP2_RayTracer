#ifndef PORTAL2RAYTRACED_PHYSICALDEVICEUTILS_H
#define PORTAL2RAYTRACED_PHYSICALDEVICEUTILS_H

#include "QueueFamilyIndices.h"
#include "Surface.h"
#include <vulkan/vulkan.hpp>

namespace roing::vk::physical_device {
	[[nodiscard]]
	QueueFamilyIndices FindQueueFamilies(const Surface &surface, VkPhysicalDevice device);

	[[nodiscard]]
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetPhysicalDeviceProperties(VkPhysicalDevice hPhysicalDevice);
}// namespace roing::vk::physical_device

#endif//PORTAL2RAYTRACED_PHYSICALDEVICEUTILS_H
