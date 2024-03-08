#ifndef PORTAL2RAYTRACED_ACCELERATIONSTRUCTURE_H
#define PORTAL2RAYTRACED_ACCELERATIONSTRUCTURE_H

#include "Buffer.h"
#include <vulkan/vulkan.hpp>

namespace roing::vk {

	struct AccelerationStructure final {
		VkDevice                   deviceHandle{};
		VkAccelerationStructureKHR accStructHandle{};
		Buffer                     buffer{};

		AccelerationStructure() = default;
		~AccelerationStructure();

		AccelerationStructure(const AccelerationStructure &)            = delete;
		AccelerationStructure &operator=(const AccelerationStructure &) = delete;

		AccelerationStructure(AccelerationStructure &&) noexcept;
		AccelerationStructure &operator=(AccelerationStructure &&) noexcept;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_ACCELERATIONSTRUCTURE_H
