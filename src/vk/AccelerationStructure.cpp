#include "AccelerationStructure.h"
#include "Device.h"
#include "src/utils/debug.h"

namespace roing::vk {
	AccelerationStructure::~AccelerationStructure() {
		if (accStructHandle == VK_NULL_HANDLE)
			return;

		DEBUG("Destroying acceleration structure...");
		roing::vk::vkDestroyAccelerationStructureKHR(deviceHandle, accStructHandle, nullptr);
		DEBUG("Destroyed acceleration structure");
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
