#include "DescriptorSetLayout.h"

namespace roing::vk {
	DescriptorSetLayout::DescriptorSetLayout(
	        const Device &device, VkDescriptorSetLayoutCreateFlags flags,
	        const std::vector<VkDescriptorBindingFlags>     &bindingFlags,
	        const std::vector<VkDescriptorSetLayoutBinding> &bindings
	)

	    : m_hDescriptorSetLayout{VK_NULL_HANDLE, DescriptorSetLayoutDestroyer{device.GetHandle()}} {
		auto bindingFlagsModified{bindingFlags};

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingsInfo{
		        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO
		};

		if (bindingFlagsModified.empty() && bindingFlagsModified.size() < bindings.size()) {
			bindingFlagsModified.resize(bindings.size(), 0);
		}

		bindingsInfo.bindingCount  = static_cast<uint32_t>(bindings.size());
		bindingsInfo.pBindingFlags = bindingFlagsModified.data();

		VkDescriptorSetLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings    = bindings.data();
		createInfo.flags        = flags;
		createInfo.pNext        = bindingFlagsModified.empty() ? nullptr : &bindingsInfo;

		VkDescriptorSetLayout descriptorSetLayout;
		if (const VkResult result{
		            vkCreateDescriptorSetLayout(device.GetHandle(), &createInfo, nullptr, &descriptorSetLayout)
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"vkCreateDescriptorSetLayout() failed: " + std::to_string(result)}};
		}

		m_hDescriptorSetLayout.reset(descriptorSetLayout);
	}

}// namespace roing::vk
