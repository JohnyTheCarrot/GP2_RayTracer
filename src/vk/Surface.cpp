#include "Surface.h"
#include "Instance.h"
#include "Window.h"
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Surface::~Surface() noexcept {
		if (m_Surface == VK_NULL_HANDLE)
			return;

		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	}

	Surface::Surface(const Instance &instance, const Window &window)
	    : m_Instance{instance.Get()} {
		if (const VkResult result{glfwCreateWindowSurface(instance.Get(), window.Get(), nullptr, &m_Surface)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create window surface: "} + string_VkResult(result)};
		}
	}

	SwapChainSupportDetails Surface::QuerySwapChainSupport(VkPhysicalDevice device) const {
		SwapChainSupportDetails details{};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(
			        device, m_Surface, &presentModeCount, details.presentModes.data()
			);
		}

		return details;
	}
}// namespace roing::vk
