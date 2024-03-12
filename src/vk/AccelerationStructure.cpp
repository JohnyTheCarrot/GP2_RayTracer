#include "AccelerationStructure.h"
#include "Device.h"
#include "src/utils/debug.h"

namespace roing::vk {
	AccelerationStructure::AccelerationStructure(
	        const Device &device, VkPhysicalDevice physicalDevice, VkAccelerationStructureCreateInfoKHR &createInfo
	)
	    : m_hAccStruct{VK_NULL_HANDLE, AccelerationStructureDestroyer(device.GetHandle())} {
		m_Buffer = device.CreateBuffer(
		        physicalDevice, createInfo.size,
		        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		);

		createInfo.buffer = m_Buffer.GetHandle();

		AccelerationStructureHandle::pointer hAccStruct;
		roing::vk::vkCreateAccelerationStructureKHR(device.GetHandle(), &createInfo, nullptr, &hAccStruct);
		m_hAccStruct.reset(hAccStruct);
	}
}// namespace roing::vk
