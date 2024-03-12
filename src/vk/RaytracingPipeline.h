#ifndef PORTAL2RAYTRACED_RAYTRACINGPIPELINE_H
#define PORTAL2RAYTRACED_RAYTRACINGPIPELINE_H

#include "DescriptorSetLayout.h"
#include "PipelineLayout.h"
#include "raii.h"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace roing::vk {

	class Device;

	class RaytracingPipeline final {
	public:
		explicit RaytracingPipeline(const roing::vk::Device &device, VkDescriptorSetLayout hDescriptorSetLayout);

		[[nodiscard]]
		PipelineHandle::pointer GetHandle() const noexcept;

	private:
		PipelineHandle                                    m_hPipeline;
		PipelineLayout                                    m_PipelineLayout;
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShaderGroups{};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_RAYTRACINGPIPELINE_H
