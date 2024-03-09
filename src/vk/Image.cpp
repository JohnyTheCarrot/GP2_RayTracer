#include "Image.h"
#include "Device.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Image::Image(VkPhysicalDevice physicalDevice, const Device &device, const VkImageCreateInfo &createInfo)
	    : m_Device{device.GetHandle()} {
		vkCreateImage(device.GetHandle(), &createInfo, nullptr, &m_Image);

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device.GetHandle(), m_Image, &memoryRequirements);

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

		vkBindImageMemory(device.GetHandle(), m_Image, memoryHandle, 0);
		m_Memory = {device, memoryHandle};
	}

	Image::~Image() {
		Destroy();
	}

	void Image::Destroy() {
		if (m_Image == VK_NULL_HANDLE)
			return;

		m_Memory.Destroy();
		vkDestroyImage(m_Device, m_Image, nullptr);
		m_Image = VK_NULL_HANDLE;
	}

	Image::Image(Image &&other) noexcept
	    : m_Device{other.m_Device}
	    , m_Image{other.m_Image}
	    , m_Memory{std::move(other.m_Memory)} {
		other.m_Image = VK_NULL_HANDLE;
	}

	Image &Image::operator=(Image &&other) noexcept {
		m_Device      = other.m_Device;
		m_Image       = other.m_Image;
		other.m_Image = VK_NULL_HANDLE;
		m_Memory      = std::move(other.m_Memory);

		return *this;
	}

	VkDeviceMemory Image::GetMemoryHandle() const noexcept {
		return m_Memory.GetHandle();
	}

	VkImage Image::GetHandle() const noexcept {
		return m_Image;
	}
}// namespace roing::vk
