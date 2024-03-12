#include "Memory.h"
#include "Device.h"
#include "src/utils/debug.h"

namespace roing::vk {
	Memory::Memory(const Device &device, VkDeviceMemory memory)
	    : m_Device{device.GetHandle()}
	    , m_Memory{memory} {
	}

	Memory::~Memory() {
		Destroy();
	}

	void Memory::Destroy() {
		if (m_Memory == VK_NULL_HANDLE)
			return;

		//		StartDestruction("Memory");
		vkFreeMemory(m_Device, m_Memory, nullptr);
		m_Memory = VK_NULL_HANDLE;
		//		EndDestruction("Memory");
	}

	Memory::Memory(Memory &&other) noexcept
	    : m_Device{other.m_Device}
	    , m_Memory{other.m_Memory} {
		other.m_Memory = VK_NULL_HANDLE;
	}

	Memory &Memory::operator=(roing::vk::Memory &&other) noexcept {
		m_Device       = other.m_Device;
		m_Memory       = other.m_Memory;
		other.m_Memory = VK_NULL_HANDLE;

		return *this;
	}

	VkDeviceMemory Memory::GetHandle() const noexcept {
		return m_Memory;
	}
}// namespace roing::vk
