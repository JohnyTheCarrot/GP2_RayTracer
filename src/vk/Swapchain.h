#ifndef PORTAL2RAYTRACED_SWAPCHAIN_H
#define PORTAL2RAYTRACED_SWAPCHAIN_H

#include "../Model.h"
#include "HostDevice.h"
#include "Texture.h"
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
		Create(Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice,
		       VkDescriptorSetLayout descriptorSetLayout);

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

		void CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout);

		// Return value: whether to recreate the swapchain
		[[nodiscard]]
		bool DrawFrame(const Device &device, roing::vk::Window &window, const std::vector<Model> &models);

		[[nodiscard]]
		VkImageView GetCurrentImageView() const noexcept;

		[[nodiscard]]
		VkImageView GetOutputImageView() const noexcept;

		[[nodiscard]]
		Texture &GetOutputTexture() noexcept;

		void CreateRtShaderBindingTable(VkPhysicalDevice physicalDevice);

		// TODO: move to device
		[[nodiscard]]
		VkShaderModule CreateShaderModule(std::vector<char> &&code);

		[[nodiscard]]
		VkExtent2D GetExtent() const noexcept;

		[[nodiscard]]
		VkFormat GetFormat() const noexcept;

		[[nodiscard]]
		VkFramebuffer GetFramebuffer() const noexcept;

		[[nodiscard]]
		VkCommandBuffer GetCommandBuffer() const noexcept;

		Buffer    m_GlobalsBuffer;
		glm::mat4 m_ViewMatrix{1.f};

	private:
		void Init(Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void CleanupOnlySwapchain();

		void RecordCommandBuffer(
		        VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<Model> &models,
		        const Window &window, const Device &device
		);

		void UpdateUniformBuffer(VkCommandBuffer commandBuffer);

		[[nodiscard]]
		static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

		[[nodiscard]]
		static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

		[[nodiscard]]
		static VkExtent2D ChooseSwapExtent(const Window &window, const VkSurfaceCapabilitiesKHR &capabilities);

		[[nodiscard]]
		static constexpr uint32_t AlignUp(uint32_t num, uint32_t alignment) {
			return (num + (alignment - 1)) & ~(alignment - 1);
		}

		[[nodiscard]]
		static VkAccessFlags AccessFlagsForImageLayout(VkImageLayout layout);

		[[nodiscard]]
		static VkPipelineStageFlags PipelineStageForLayout(VkImageLayout layout);

		static void CmdBarrierImageLayout(
		        VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
		        const VkImageSubresourceRange &subresourceRange
		);

		static void CmdBarrierImageLayout(
		        VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
		        VkImageAspectFlags imageAspectFlags
		);

		uint32_t                   m_CurrentFrame{0};
		Device                    *m_pParentDevice{};
		VkSwapchainKHR             m_SwapChain{};
		VkFormat                   m_SwapChainImageFormat{};
		VkExtent2D                 m_SwapChainExtent{};
		std::vector<VkImage>       m_SwapChainImages{};
		std::vector<VkImageView>   m_SwapChainImageViews{};
		std::vector<VkFramebuffer> m_SwapChainFramebuffers{};
		VkRenderPass               m_RenderPass{};

		std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShaderGroups{};
		VkPipeline                                        m_RayTracingPipeline{};
		VkPipelineLayout                                  m_PipelineLayout{};
		PushConstantRay                                   m_PushConstantRay{};
		Buffer                                            m_SBTBuffer{};
		VkStridedDeviceAddressRegionKHR                   m_RgenRegion{};
		VkStridedDeviceAddressRegionKHR                   m_MissRegion{};
		VkStridedDeviceAddressRegionKHR                   m_HitRegion{};
		VkStridedDeviceAddressRegionKHR                   m_CallRegion{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR   m_RtProperties{
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        };

		Texture     m_OutImage;
		VkImageView m_OutImageView{};


		static constexpr int MAX_FRAMES_IN_FLIGHT{2};

		std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     m_RenderFinishedSemaphores{};
		std::array<VkFence, MAX_FRAMES_IN_FLIGHT>         m_InFlightFences{};
		std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_CommandBuffers{};
		std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     m_ImageAvailableSemaphores{};
	};

}// namespace roing::vk

void OnKey(GLFWwindow *pSwapchain, int key, int scancode, int action, int mods);

#endif//PORTAL2RAYTRACED_SWAPCHAIN_H
