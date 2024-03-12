#include "Image.h"
#include "Device.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Image::Image(VkPhysicalDevice physicalDevice, const Device &device, const VkImageCreateInfo &createInfo)
	    : m_Image{VK_NULL_HANDLE, ImageDestroyer(device.GetHandle())} {
		VkImage image;
		vkCreateImage(device.GetHandle(), &createInfo, nullptr, &image);
		m_Image.reset(image);

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device.GetHandle(), GetHandle(), &memoryRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize  = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = Device::FindMemoryType(
		        physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		VkDeviceMemory memoryHandle;
		if (const VkResult result{vkAllocateMemory(device.GetHandle(), &memoryAllocateInfo, nullptr, &memoryHandle)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to allocate memory: "} + string_VkResult(result)};
		}

		vkBindImageMemory(device.GetHandle(), GetHandle(), memoryHandle, 0);
		m_Memory = {device, memoryHandle};
	}

	VkDeviceMemory Image::GetMemoryHandle() const noexcept {
		return m_Memory.GetHandle();
	}

	VkImage Image::GetHandle() const noexcept {
		return m_Image.get();
	}

	void Image::ChangeImageLayout(
	        const CommandBuffer &commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout,
	        const VkImageSubresourceRange &subresourceRange
	) {
		VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imageMemoryBarrier.oldLayout        = oldLayout;
		imageMemoryBarrier.newLayout        = newLayout;
		imageMemoryBarrier.image            = m_Image.get();
		imageMemoryBarrier.subresourceRange = subresourceRange;
		imageMemoryBarrier.srcAccessMask    = AccessFlagsForImageLayout(oldLayout);
		imageMemoryBarrier.dstAccessMask    = AccessFlagsForImageLayout(newLayout);

		const VkPipelineStageFlags srcStageMask{PipelineStageForLayout(oldLayout)};
		const VkPipelineStageFlags dstStageMask{PipelineStageForLayout(newLayout)};
		vkCmdPipelineBarrier(
		        commandBuffer.GetHandle(), srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier
		);
	}

	void Image::ChangeImageLayout(
	        const CommandBuffer &commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout,
	        VkImageAspectFlags imageAspectFlags
	) {
		VkImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask     = imageAspectFlags;
		subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
		subresourceRange.baseMipLevel   = 0;
		subresourceRange.baseArrayLayer = 0;
		ChangeImageLayout(commandBuffer, oldLayout, newLayout, subresourceRange);
	}

	VkAccessFlags Image::AccessFlagsForImageLayout(VkImageLayout layout) {
		switch (layout) {
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return VK_ACCESS_HOST_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_ACCESS_TRANSFER_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_ACCESS_TRANSFER_READ_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_ACCESS_SHADER_READ_BIT;
			default:
				return VkAccessFlags();
		}
	}

	VkPipelineStageFlags Image::PipelineStageForLayout(VkImageLayout layout) {
		switch (layout) {
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return VK_PIPELINE_STAGE_HOST_BIT;
			case VK_IMAGE_LAYOUT_UNDEFINED:
				return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			default:
				return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}
	}
}// namespace roing::vk
