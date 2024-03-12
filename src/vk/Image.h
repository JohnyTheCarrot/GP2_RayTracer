#ifndef PORTAL2RAYTRACED_IMAGE_H
#define PORTAL2RAYTRACED_IMAGE_H

#include "CommandBuffer.h"
#include "Memory.h"
#include "raii.h"
#include <vulkan/vulkan.hpp>

namespace roing::vk {
	class Device;

	class Image final {
	public:
		Image(VkPhysicalDevice physicalDevice, const Device &device, const VkImageCreateInfo &createInfo);

		[[nodiscard]]
		VkDeviceMemory GetMemoryHandle() const noexcept;

		[[nodiscard]]
		VkImage GetHandle() const noexcept;

		void ChangeImageLayout(
		        const roing::vk::CommandBuffer &commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout,
		        const VkImageSubresourceRange &subresourceRange
		);

		void ChangeImageLayout(
		        const CommandBuffer &commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout,
		        VkImageAspectFlags imageAspectFlags
		);

	private:
		[[nodiscard]]
		static VkAccessFlags AccessFlagsForImageLayout(VkImageLayout layout);

		[[nodiscard]]
		static VkPipelineStageFlags PipelineStageForLayout(VkImageLayout layout);

		ImageHandle m_Image;
		Memory      m_Memory;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_IMAGE_H
