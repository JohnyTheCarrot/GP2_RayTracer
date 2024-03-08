#ifndef PORTAL2RAYTRACED_SURFACE_H
#define PORTAL2RAYTRACED_SURFACE_H

#include <vector>
#include <vulkan/vulkan.h>

namespace roing::vk {

	class Instance;

	class Window;

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR        capabilities{};
		std::vector<VkSurfaceFormatKHR> formats{};
		std::vector<VkPresentModeKHR>   presentModes{};
	};

	class Surface final {
	public:
		Surface() = default;

		Surface(const Instance &instance, const Window &window);

		~Surface() noexcept;

		Surface(const Surface &)            = delete;
		Surface &operator=(const Surface &) = delete;

		Surface(Surface &&other) noexcept
		    : m_Surface{other.m_Surface}
		    , m_Instance{other.m_Instance} {
			other.m_Surface  = nullptr;
			other.m_Instance = nullptr;
		}

		Surface &operator=(Surface &&other) noexcept {
			m_Surface        = other.m_Surface;
			m_Instance       = other.m_Instance;
			other.m_Surface  = nullptr;
			other.m_Instance = nullptr;

			return *this;
		}

		[[nodiscard]]
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;

		[[nodiscard]]
		VkSurfaceKHR Get() const noexcept {
			return m_Surface;
		}

	private:
		VkSurfaceKHR m_Surface{};
		VkInstance   m_Instance{};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_SURFACE_H
