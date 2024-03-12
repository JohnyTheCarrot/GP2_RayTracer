#ifndef PORTAL2RAYTRACED_SWAPCHAIN_H
#define PORTAL2RAYTRACED_SWAPCHAIN_H

#include "raii.h"
#include <vector>

namespace roing::vk {

	class Surface;

	class Device;

	class Window;

	class Swapchain final {
	public:
		Swapchain(const Surface &surface, const Window &window, const Device &device, VkPhysicalDevice hPhysicalDevice);

		[[nodiscard]]
		SwapchainHandle::pointer GetHandle() const noexcept;

		[[nodiscard]]
		std::vector<VkImage> GetImages() const;

		[[nodiscard]]
		VkFormat GetImageFormat() const noexcept;

		[[nodiscard]]
		VkExtent2D GetExtent() const noexcept;

	private:
		[[nodiscard]]
		static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

		[[nodiscard]]
		static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

		[[nodiscard]]
		static VkExtent2D ChooseSwapExtent(const Window &window, const VkSurfaceCapabilitiesKHR &capabilities);

		SwapchainHandle       m_hSwapchain;
		DeviceHandle::pointer m_hDevice;
		VkFormat              m_SwapchainImageFormat;
		VkExtent2D            m_SwapChainExtent;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_SWAPCHAIN_H
