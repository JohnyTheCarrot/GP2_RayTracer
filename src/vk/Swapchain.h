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

		Swapchain(Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		~Swapchain();

		Swapchain(const Swapchain &)            = delete;
		Swapchain &operator=(const Swapchain &) = delete;

		Swapchain(Swapchain &&other) noexcept
		    : m_pParentDevice{other.m_pParentDevice}
		    , m_SwapChain{other.m_SwapChain}
		    , m_SwapChainImageFormat{other.m_SwapChainImageFormat}
		    , m_SwapChainExtent{other.m_SwapChainExtent}
		    , m_SwapChainImages{std::move(other.m_SwapChainImages)}
		    , m_SwapChainImageViews{std::move(other.m_SwapChainImageViews)}
		    , m_SwapChainFramebuffers{std::move(other.m_SwapChainFramebuffers)} {
			other.m_pParentDevice = nullptr;
		}

		Swapchain &operator=(Swapchain &&other) noexcept {
			m_pParentDevice       = other.m_pParentDevice;
			other.m_pParentDevice = nullptr;

			m_SwapChain             = other.m_SwapChain;
			m_SwapChainImageFormat  = other.m_SwapChainImageFormat;
			m_SwapChainExtent       = other.m_SwapChainExtent;
			m_SwapChainImages       = std::move(other.m_SwapChainImages);
			m_SwapChainImageViews   = std::move(other.m_SwapChainImageViews);
			m_SwapChainFramebuffers = std::move(other.m_SwapChainFramebuffers);

			return *this;
		}

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

	private:
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
