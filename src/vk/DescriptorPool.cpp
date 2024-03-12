#include "DescriptorPool.h"

#include "Device.h"

namespace roing::vk {
	DescriptorPool::DescriptorPool(const Device &device, const VkDescriptorPoolCreateInfo &createInfo)
	    : m_hDescriptorPool{VK_NULL_HANDLE, DescriptorPoolDestroyer{device.GetHandle()}} {
		VkDescriptorPool descriptorPool;
		VkResult         result{vkCreateDescriptorPool(device.GetHandle(), &createInfo, nullptr, &descriptorPool)};
		assert(result == VK_SUCCESS);
		m_hDescriptorPool.reset(descriptorPool);
	}

	DescriptorSetHandle::pointer DescriptorPool::GetHandle() const noexcept {
		return m_hDescriptorPool.get();
	}
}// namespace roing::vk
