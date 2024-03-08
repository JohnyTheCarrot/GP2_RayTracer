#include "AccelerationStructure.h"
#include "Device.h"

namespace roing::vk {
	AccelerationStructure::~AccelerationStructure() {
		if (accStructHandle == VK_NULL_HANDLE)
			return;

		roing::vk::vkDestroyAccelerationStructureKHR(deviceHandle, accStructHandle, nullptr);
	}

	AccelerationStructure::AccelerationStructure(AccelerationStructure &&other) noexcept
	    : deviceHandle{other.deviceHandle}
	    , accStructHandle{other.accStructHandle}
	    , buffer{std::move(other.buffer)} {
		other.accStructHandle = nullptr;
	}

	AccelerationStructure &AccelerationStructure::operator=(AccelerationStructure &&other) noexcept {
		deviceHandle    = other.deviceHandle;
		accStructHandle = other.accStructHandle;
		buffer          = std::move(other.buffer);

		other.accStructHandle = nullptr;
		return *this;
	}
}// namespace roing::vk
