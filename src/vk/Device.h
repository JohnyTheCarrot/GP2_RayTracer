#ifndef PORTAL2RAYTRACED_DEVICE_H
#define PORTAL2RAYTRACED_DEVICE_H

#include "Surface.h"
#include "Swapchain.h"
#include <array>
#include <vulkan/vulkan.h>

namespace roing::vk {
	class Device final {
	public:
		Device() = default;

		explicit Device(const Surface &surface, VkPhysicalDevice physicalDevice);

		~Device() noexcept;

		Device(const Device &)            = delete;
		Device &operator=(const Device &) = delete;

		Device(Device &&other) noexcept
		    : m_Device{other.m_Device}
		    , m_CommandPool{other.m_CommandPool}
		    , m_GraphicsQueue{other.m_GraphicsQueue}
		    , m_PresentQueue{other.m_PresentQueue}
		    , m_SwapChain{std::move(other.m_SwapChain)} {
			other.m_Device = nullptr;
		};

		Device &operator=(Device &&other) noexcept {
			m_Device        = other.m_Device;
			other.m_Device  = nullptr;
			m_CommandPool   = other.m_CommandPool;
			m_GraphicsQueue = other.m_GraphicsQueue;
			m_PresentQueue  = other.m_PresentQueue;
			m_SwapChain     = std::move(other.m_SwapChain);

			return *this;
		}

		[[nodiscard]]
		Swapchain &GetSwapchain() noexcept {
			return m_SwapChain;
		}

		[[nodiscard]]
		VkDevice GetHandle() const noexcept {
			return m_Device;
		}

		Swapchain &CreateSwapchain(const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void RecreateSwapchain(const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

		void QueueSubmit(uint32_t submitCount, const VkSubmitInfo *pSubmitInfo, VkFence fence);

		// Return value: whether to recreate the swap chain
		bool QueuePresent(const VkPresentInfoKHR *pPresentInfo, Window &window);

		static constexpr std::array<const char *, 6> DEVICE_EXTENSIONS{
		        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		        VK_KHR_SPIRV_1_4_EXTENSION_NAME
		};

		void CreateCommandPool(const Surface &surface, VkPhysicalDevice physicalDevice);

		void CreateBuffer(
		        VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
		        VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory
		) const;

		[[nodiscard]]
		VkCommandPool GetCommandPoolHandle() const noexcept {
			return m_CommandPool;
		}

	private:
		static uint32_t
		FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

		static constexpr std::array<const char *, 1> VALIDATION_LAYERS{"VK_LAYER_KHRONOS_validation"};

		VkDevice      m_Device{};
		VkCommandPool m_CommandPool{};
		VkQueue       m_GraphicsQueue{};
		VkQueue       m_PresentQueue{};
		Swapchain     m_SwapChain{};
	};
}// namespace roing::vk

#endif//PORTAL2RAYTRACED_DEVICE_H
