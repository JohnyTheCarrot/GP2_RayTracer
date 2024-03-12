#include "Buffer.h"
#include "Device.h"
#include "src/utils/debug.h"
#include <iostream>

namespace roing::vk {
	Buffer::~Buffer() {
		Destroy();
	}

	void Buffer::Destroy() {
		if (m_Buffer == VK_NULL_HANDLE)
			return;

		//		StartDestruction("Buffer");
		m_BufferMemory.Destroy();
		vkDestroyBuffer(m_Device, m_Buffer, nullptr);
		m_Buffer = VK_NULL_HANDLE;
		//		EndDestruction("Buffer");
	};

	Buffer::Buffer(const Device &device, VkBuffer buffer, VkDeviceMemory memory)
	    : m_Device{device.GetHandle()}
	    , m_Buffer{buffer}
	    , m_BufferMemory{device, memory} {
	}

	VkDeviceAddress Buffer::GetDeviceAddress() const noexcept {
		VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_Buffer};
		return vkGetBufferDeviceAddress(m_Device, &bufferInfo);
	}
}// namespace roing::vk
