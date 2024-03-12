#include "PipelineLayout.h"
#include "Device.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	PipelineLayout::PipelineLayout(
	        const Device &device, VkDescriptorSetLayout descriptorSetLayout, VkPushConstantRange pushConstantRange
	)
	    : m_Device{device.GetHandle()}
	    , m_hPipelineLayout{VK_NULL_HANDLE, PipelineLayoutDestroyer{device.GetHandle()}} {
		VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		createInfo.setLayoutCount         = 1;
		createInfo.pSetLayouts            = &descriptorSetLayout;
		createInfo.pushConstantRangeCount = 1;
		createInfo.pPushConstantRanges    = &pushConstantRange;

		VkPipelineLayout pipelineLayout;
		if (const VkResult result{vkCreatePipelineLayout(device.GetHandle(), &createInfo, nullptr, &pipelineLayout)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create pipeline layout: "} + string_VkResult(result)};
		}
		m_hPipelineLayout.reset(pipelineLayout);
	}

	PipelineLayoutHandle::pointer PipelineLayout::GetHandle() const noexcept {
		return m_hPipelineLayout.get();
	}
}// namespace roing::vk
