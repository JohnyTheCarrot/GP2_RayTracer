#ifndef PORTAL2RAYTRACED_SWAPCHAIN_H
#define PORTAL2RAYTRACED_SWAPCHAIN_H

#include "../Model.h"
#include "Window.h"
#include <vector>
#include <vulkan/vulkan.hpp>

namespace roing::vk {

	class Device;

	class Surface;

	class Swapchain final {
	public:
		Swapchain() = default;

		~Swapchain();

		void
		Create(Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void
		Recreate(Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void Cleanup();

		Swapchain(const Swapchain &)            = delete;
		Swapchain &operator=(const Swapchain &) = delete;

		Swapchain(Swapchain &&)            = delete;
		Swapchain &operator=(Swapchain &&) = delete;

		[[nodiscard]]
		VkSwapchainKHR GetHandle() const noexcept {
			return m_SwapChain;
		}

		void CreateFramebuffers();

		void CreateCommandBuffers();

		void CreateSyncObjects();

		void CreateImageViews();

		void CreateRenderPass();

		void CreateGraphicsPipeline();

		// Return value: whether to recreate the swapchain
		[[nodiscard]]
		bool DrawFrame(roing::vk::Window &window, const std::vector<Model> &models);

		[[nodiscard]]
		VkImageView GetCurrentImageView() const noexcept;

	private:
		void Init(Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void CleanupOnlySwapchain();

		[[nodiscard]]
		VkShaderModule CreateShaderModule(std::vector<char> &&code);

		void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<Model> &models);

		[[nodiscard]]
		static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

		[[nodiscard]]
		static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

		[[nodiscard]]
		static VkExtent2D ChooseSwapExtent(const Window &window, const VkSurfaceCapabilitiesKHR &capabilities);

		uint32_t                   m_CurrentFrame{0};
		Device                    *m_pParentDevice{};
		VkSwapchainKHR             m_SwapChain{};
		VkFormat                   m_SwapChainImageFormat{};
		VkExtent2D                 m_SwapChainExtent{};
		std::vector<VkImage>       m_SwapChainImages{};
		std::vector<VkImageView>   m_SwapChainImageViews{};
		std::vector<VkFramebuffer> m_SwapChainFramebuffers{};
		VkRenderPass               m_RenderPass{};
		VkPipeline                 m_GraphicsPipeline{};
		VkPipelineLayout           m_PipelineLayout{};

		static constexpr int MAX_FRAMES_IN_FLIGHT{2};

		std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     m_RenderFinishedSemaphores{};
		std::array<VkFence, MAX_FRAMES_IN_FLIGHT>         m_InFlightFences{};
		std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_CommandBuffers{};
		std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     m_ImageAvailableSemaphores{};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_SWAPCHAIN_H
