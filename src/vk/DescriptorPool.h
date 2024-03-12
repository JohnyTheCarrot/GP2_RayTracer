#ifndef PORTAL2RAYTRACED_DESCRIPTORPOOL_H
#define PORTAL2RAYTRACED_DESCRIPTORPOOL_H

#include "raii.h"
#include <vector>
#include <vulkan/vulkan.hpp>

namespace roing::vk {

	class Device;

	class DescriptorPool final {
	public:
		DescriptorPool(const Device &device, const VkDescriptorPoolCreateInfo &createInfo);

		[[nodiscard]]
		DescriptorPoolHandle::pointer GetHandle() const noexcept;

	private:
		DescriptorPoolHandle m_hDescriptorPool;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_DESCRIPTORPOOL_H
