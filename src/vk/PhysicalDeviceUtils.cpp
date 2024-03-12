#include "PhysicalDeviceUtils.h"

namespace roing::vk::physical_device {
	QueueFamilyIndices FindQueueFamilies(const Surface &surface, VkPhysicalDevice device) {
		QueueFamilyIndices indices{};

		uint32_t queueFamilyCount{0};
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (int idx{}; idx < queueFamilies.size(); ++idx) {
			VkBool32 presentSupport{false};
			vkGetPhysicalDeviceSurfaceSupportKHR(device, idx, surface.Get(), &presentSupport);

			if (presentSupport) {
				indices.presentFamily = idx;
			}

			if (queueFamilies[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = idx;
			}

			if (indices.IsComplete()) {
				break;
			}
		}

		return indices;
	}

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetPhysicalDeviceProperties(VkPhysicalDevice hPhysicalDevice) {
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR physicalDeviceProperties{};

		VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		properties2.pNext = &physicalDeviceProperties;
		vkGetPhysicalDeviceProperties2(hPhysicalDevice, &properties2);

		return physicalDeviceProperties;
	}
}// namespace roing::vk::physical_device
