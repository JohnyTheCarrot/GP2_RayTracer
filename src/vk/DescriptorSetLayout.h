#ifndef PORTAL2RAYTRACED_DESCRIPTORSETLAYOUT_H
#define PORTAL2RAYTRACED_DESCRIPTORSETLAYOUT_H

#include "Device.h"
#include "raii.h"
#include "vulkan/vulkan.hpp"

namespace roing::vk {

	class DescriptorSetLayout final {
	public:
		DescriptorSetLayout(
		        const Device &device, VkDescriptorSetLayoutCreateFlags flags,
		        const std::vector<VkDescriptorBindingFlags>     &bindingFlags,
		        const std::vector<VkDescriptorSetLayoutBinding> &bindings
		);

		[[nodiscard]]
		DescriptorSetLayoutHandle::pointer GetHandle() const noexcept {
			return m_hDescriptorSetLayout.get();
		}

	private:
		DescriptorSetLayoutHandle m_hDescriptorSetLayout;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_DESCRIPTORSETLAYOUT_H
