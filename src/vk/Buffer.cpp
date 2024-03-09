#include "Buffer.h"
#include "Device.h"
#include "src/utils/debug.h"
#include <iostream>

namespace roing::vk {
	Buffer::~Buffer() {
		if (m_Buffer == VK_NULL_HANDLE)
			return;

		DEBUG("Destroying buffer and freeing buffer memory");
		vkDestroyBuffer(m_Device, m_Buffer, nullptr);
		vkFreeMemory(m_Device, m_BufferMemory, nullptr);
	}

	Buffer::Buffer(const Device &device, VkBuffer buffer, VkDeviceMemory memory)
	    : m_Device{device.GetHandle()}
	    , m_Buffer{buffer}
	    , m_BufferMemory{memory} {
	}

	VkDeviceAddress Buffer::GetDeviceAddress() const noexcept {
		VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_Buffer};
		return vkGetBufferDeviceAddress(m_Device, &bufferInfo);
	};
}// namespace roing::vk
