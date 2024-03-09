#ifndef PORTAL2RAYTRACED_IMAGE_H
#define PORTAL2RAYTRACED_IMAGE_H

#include "Memory.h"
#include <vulkan/vulkan.hpp>

namespace roing::vk {
	class Device;

	class Image final {
	public:
		Image() = default;

		Image(VkPhysicalDevice physicalDevice, const Device &device, const VkImageCreateInfo &createInfo);

		~Image();

		void Destroy();

		Image(const Image &)            = delete;
		Image &operator=(const Image &) = delete;

		Image(Image &&other) noexcept;
		Image &operator=(Image &&other) noexcept;

		[[nodiscard]]
		VkDeviceMemory GetMemoryHandle() const noexcept;

		[[nodiscard]]
		VkImage GetHandle() const noexcept;

	private:
		VkDevice m_Device;
		VkImage  m_Image;
		Memory   m_Memory;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_IMAGE_H
