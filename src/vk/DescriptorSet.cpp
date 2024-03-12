#include "DescriptorSet.h"
#include "DescriptorPool.h"

namespace roing::vk {
	DescriptorSet::DescriptorSet(
	        const Device &device, const DescriptorPool &descriptorPool, const DescriptorSetLayout &descriptorSetLayout
	)
	    : m_hDescriptorSet{VK_NULL_HANDLE, DescriptorSetDestroyer(device.GetHandle(), descriptorPool.GetHandle())} {
		std::array<VkDescriptorSetLayout, 1> layouts{descriptorSetLayout.GetHandle()};

		VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocateInfo.descriptorPool     = descriptorPool.GetHandle();
		allocateInfo.descriptorSetCount = layouts.size();
		allocateInfo.pSetLayouts        = layouts.data();

		VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
		vkAllocateDescriptorSets(device.GetHandle(), &allocateInfo, &descriptorSet);
		m_hDescriptorSet.reset(descriptorSet);
	}
}// namespace roing::vk
