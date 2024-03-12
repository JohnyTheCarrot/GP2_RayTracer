#ifndef PORTAL2RAYTRACED_ACCELERATIONSTRUCTURE_H
#define PORTAL2RAYTRACED_ACCELERATIONSTRUCTURE_H

#include "Buffer.h"
#include "raii.h"
#include <vulkan/vulkan.hpp>

namespace roing::vk {

	class Device;

	struct AccelerationStructure final {
		AccelerationStructureHandle m_hAccStruct;
		Buffer                      m_Buffer{};

		AccelerationStructure(
		        const Device &device, VkPhysicalDevice physicalDevice, VkAccelerationStructureCreateInfoKHR &createInfo
		);
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_ACCELERATIONSTRUCTURE_H
