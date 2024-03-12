#include "Swapchain.h"
#include "Device.h"
#include "PhysicalDeviceUtils.h"
#include "Surface.h"
#include "Window.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Swapchain::Swapchain(
	        const Surface &surface, const Window &window, const Device &device, VkPhysicalDevice hPhysicalDevice
	)
	    : m_hSwapchain{VK_NULL_HANDLE, SwapchainDestroyer{device.GetHandle()}}
	    , m_hDevice{device.GetHandle()} {
		const SwapChainSupportDetails swapChainSupport{surface.QuerySwapChainSupport(hPhysicalDevice)};

		const VkSurfaceFormatKHR surfaceFormat{ChooseSwapSurfaceFormat(swapChainSupport.formats)};
		const VkPresentModeKHR   presentMode{ChooseSwapPresentMode(swapChainSupport.presentModes)};
		const VkExtent2D         extent{ChooseSwapExtent(window, swapChainSupport.capabilities)};

		m_SwapchainImageFormat = surfaceFormat.format;
		m_SwapChainExtent      = extent;

		uint32_t imageCount{swapChainSupport.capabilities.minImageCount + 1};

		if (swapChainSupport.capabilities.maxImageCount > 0 &&
		    imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.minImageCount    = imageCount;
		createInfo.surface          = surface.Get();
		createInfo.imageFormat      = surfaceFormat.format;
		createInfo.imageColorSpace  = surfaceFormat.colorSpace;
		createInfo.imageExtent      = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
		                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		vk::QueueFamilyIndices  indices{vk::physical_device::FindQueueFamilies(surface, hPhysicalDevice)};
		std::array<uint32_t, 2> queueFamilyIndices{indices.graphicsFamily.value(), indices.presentFamily.value()};

		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
		} else {
			createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices   = nullptr;
		}

		createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		createInfo.presentMode = presentMode;
		createInfo.clipped     = VK_TRUE;

		createInfo.oldSwapchain = VK_NULL_HANDLE;

		VkSwapchainKHR swapChain;
		if (const VkResult result{vkCreateSwapchainKHR(device.GetHandle(), &createInfo, nullptr, &swapChain)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create swap chain: "} + string_VkResult(result)};
		}
		m_hSwapchain.reset(swapChain);
	}

	VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
		const auto formatIterator{std::find_if(
		        availableFormats.cbegin(), availableFormats.cend(),
		        [](const VkSurfaceFormatKHR &format) {
			        return format.format == VK_FORMAT_R32G32B32A32_SFLOAT &&
			               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		        }
		)};

		if (formatIterator != availableFormats.cend()) {
			return *formatIterator;
		}

		return availableFormats.front();
	}

	VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
		const auto presentModeIterator{std::find_if(
		        availablePresentModes.cbegin(), availablePresentModes.cend(),
		        [](const VkPresentModeKHR &presentMode) { return presentMode == VK_PRESENT_MODE_MAILBOX_KHR; }
		)};

		if (presentModeIterator != availablePresentModes.cend()) {
			return *presentModeIterator;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D Swapchain::ChooseSwapExtent(const Window &window, const VkSurfaceCapabilitiesKHR &capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;

		int width, height;
		glfwGetFramebufferSize(window.Get(), &width, &height);

		VkExtent2D actualExtent{
		        .width  = static_cast<uint32_t>(width),
		        .height = static_cast<uint32_t>(height),
		};

		actualExtent.width =
		        std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);

		actualExtent.height =
		        std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}

	SwapchainHandle::pointer Swapchain::GetHandle() const noexcept {
		return m_hSwapchain.get();
	}

	std::vector<VkImage> Swapchain::GetImages() const {
		uint32_t imageCount{};
		if (const VkResult result{vkGetSwapchainImagesKHR(m_hDevice, m_hSwapchain.get(), &imageCount, nullptr)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{
			        std::string{"initial vkGetSwapchainImagesKHR() failed: "} + string_VkResult(result)
			};
		}

		std::vector<VkImage> swapChainImages(imageCount);
		if (const VkResult result{
		            vkGetSwapchainImagesKHR(m_hDevice, m_hSwapchain.get(), &imageCount, swapChainImages.data())
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"vkGetSwapchainImagesKHR() failed: "} + string_VkResult(result)};
		}

		return swapChainImages;
	}

	VkFormat Swapchain::GetImageFormat() const noexcept {
		return m_SwapchainImageFormat;
	}

	VkExtent2D Swapchain::GetExtent() const noexcept {
		return m_SwapChainExtent;
	}
}// namespace roing::vk
