#ifndef PORTAL2RAYTRACED_PIPELINELAYOUT_H
#define PORTAL2RAYTRACED_PIPELINELAYOUT_H

#include "raii.h"
#include <vulkan/vulkan_core.h>

namespace roing::vk {

	class Device;

	class PipelineLayout final {
	public:
		PipelineLayout(
		        const Device &device, VkDescriptorSetLayout descriptorSetLayout, VkPushConstantRange pushConstantRange
		);

		[[nodiscard]]
		PipelineLayoutHandle::pointer GetHandle() const noexcept;

	private:
		VkDevice             m_Device;
		PipelineLayoutHandle m_hPipelineLayout;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_PIPELINELAYOUT_H
