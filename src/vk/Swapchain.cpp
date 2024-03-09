#include "Swapchain.h"
#include "../utils/read_file.h"
#include "Device.h"
#include "PhysicalDeviceUtils.h"
#include "QueueFamilyIndices.h"
#include "RayTracingExtFunctions.h"
#include "src/utils/debug.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Swapchain::~Swapchain() {
		Cleanup();
	}

	void Swapchain::Cleanup() {
		if (m_pParentDevice == nullptr || m_pParentDevice->GetHandle() == VK_NULL_HANDLE)
			return;

		DEBUG("Destroying swapchain...");
		CleanupOnlySwapchain();
		m_SBTBuffer.Destroy();

		for (size_t i{0}; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vkDestroySemaphore(m_pParentDevice->GetHandle(), m_ImageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(m_pParentDevice->GetHandle(), m_RenderFinishedSemaphores[i], nullptr);
			vkDestroyFence(m_pParentDevice->GetHandle(), m_InFlightFences[i], nullptr);
		}

		vkDestroyPipeline(m_pParentDevice->GetHandle(), m_RayTracingPipeline, nullptr);

		vkDestroyPipelineLayout(m_pParentDevice->GetHandle(), m_PipelineLayout, nullptr);

		vkDestroyRenderPass(m_pParentDevice->GetHandle(), m_RenderPass, nullptr);

		DEBUG("Swapchain destroyed");
		m_pParentDevice = nullptr;
	}

	void Swapchain::CleanupOnlySwapchain() {
		if (m_pParentDevice->GetHandle() == VK_NULL_HANDLE)
			return;

		vkDestroyImageView(m_pParentDevice->GetHandle(), m_OutImageView, nullptr);
		m_OutImage.m_Image.Destroy();

		for (auto framebuffer: m_SwapChainFramebuffers) {
			vkDestroyFramebuffer(m_pParentDevice->GetHandle(), framebuffer, nullptr);
		}

		for (auto imageView: m_SwapChainImageViews) {
			vkDestroyImageView(m_pParentDevice->GetHandle(), imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_pParentDevice->GetHandle(), m_SwapChain, nullptr);
	}

	void Swapchain::Init(
	        Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice
	) {
		VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		properties2.pNext = &m_RtProperties;
		vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);

		m_pParentDevice = pParentDevice;
		const vk::SwapChainSupportDetails swapChainSupport{surface.QuerySwapChainSupport(physicalDevice)};

		const VkSurfaceFormatKHR surfaceFormat{ChooseSwapSurfaceFormat(swapChainSupport.formats)};
		const VkPresentModeKHR   presentMode{ChooseSwapPresentMode(swapChainSupport.presentModes)};
		const VkExtent2D         extent{ChooseSwapExtent(window, swapChainSupport.capabilities)};

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

		vk::QueueFamilyIndices  indices{vk::physical_device::FindQueueFamilies(surface, physicalDevice)};
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

		if (const VkResult result{vkCreateSwapchainKHR(pParentDevice->GetHandle(), &createInfo, nullptr, &m_SwapChain)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create swap chain: "} + string_VkResult(result)};
		}

		vkGetSwapchainImagesKHR(pParentDevice->GetHandle(), m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(pParentDevice->GetHandle(), m_SwapChain, &imageCount, m_SwapChainImages.data());

		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent      = extent;

		VkImageCreateInfo imageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		imageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format        = m_SwapChainImageFormat;
		imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.mipLevels     = 1;
		imageCreateInfo.arrayLayers   = 1;
		imageCreateInfo.extent.width  = m_SwapChainExtent.width;
		imageCreateInfo.extent.height = m_SwapChainExtent.height;
		imageCreateInfo.extent.depth  = 1;
		imageCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		                        VK_IMAGE_USAGE_STORAGE_BIT;

		m_OutImage.m_Image                = {physicalDevice, *m_pParentDevice, imageCreateInfo};
		m_OutImage.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		samplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.anisotropyEnable        = VK_FALSE;
		samplerCreateInfo.maxAnisotropy           = 1.f;
		samplerCreateInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable           = VK_FALSE;
		samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		if (const VkResult result{vkCreateSampler(
		            m_pParentDevice->GetHandle(), &samplerCreateInfo, nullptr, &m_OutImage.descriptor.sampler
		    )};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create sampler: "} + string_VkResult(result)};
		}

		VkCommandBuffer cmdBuf{m_pParentDevice->CreateCommandBuffer()};
		CmdBarrierImageLayout(
		        cmdBuf, m_OutImage.m_Image.GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		        VK_IMAGE_ASPECT_COLOR_BIT
		);
		m_pParentDevice->SubmitAndWait(cmdBuf);

		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.image                           = m_OutImage.m_Image.GetHandle();
		viewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format                          = m_SwapChainImageFormat;
		viewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.subresourceRange.baseMipLevel   = 0;
		viewCreateInfo.subresourceRange.levelCount     = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount     = 1;

		if (const VkResult result{
		            vkCreateImageView(m_pParentDevice->GetHandle(), &viewCreateInfo, nullptr, &m_OutImageView)
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create image view: "} + string_VkResult(result)};
		}

		m_OutImage.descriptor.imageView = m_OutImageView;
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

	void Swapchain::CreateFramebuffers() {
		m_SwapChainFramebuffers.resize(m_SwapChainImages.size());

		for (uint32_t i{0}; i < m_SwapChainImageViews.size(); i++) {
			std::array<VkImageView, 1> attachments{m_SwapChainImageViews[i]};

			VkFramebufferCreateInfo framebufferCreateInfo{};
			framebufferCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass      = m_pParentDevice->m_RenderPass;
			framebufferCreateInfo.attachmentCount = attachments.size();
			framebufferCreateInfo.pAttachments    = attachments.data();
			framebufferCreateInfo.width           = m_SwapChainExtent.width;
			framebufferCreateInfo.height          = m_SwapChainExtent.height;
			framebufferCreateInfo.layers          = 1;

			if (const VkResult result{vkCreateFramebuffer(
			            m_pParentDevice->GetHandle(), &framebufferCreateInfo, nullptr, &m_SwapChainFramebuffers[i]
			    )};
			    result != VK_SUCCESS) {
				throw std::runtime_error{std::string{"Failed to create framebuffer: "} + string_VkResult(result)};
			}
		}
	}

	void Swapchain::RecordCommandBuffer(
	        VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<Model> &models, const Window &window,
	        const Device &device
	) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags            = 0;
		beginInfo.pInheritanceInfo = nullptr;

		if (const VkResult result{vkBeginCommandBuffer(commandBuffer, &beginInfo)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to begin command buffer: "} + string_VkResult(result)};
		}

		uint32_t presentFamilyIndex{m_pParentDevice->GetQueueFamilyIndices().presentFamily.value()};

		VkImageSubresourceRange imageSubresourceRange{
		        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		        .baseMipLevel   = 0,
		        .levelCount     = 1,
		        .baseArrayLayer = 0,
		        .layerCount     = 1
		};

		VkImageMemoryBarrier fromPresentToClear{
		        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		        .pNext               = nullptr,
		        .srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
		        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
		        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
		        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		        .srcQueueFamilyIndex = presentFamilyIndex,
		        .dstQueueFamilyIndex = presentFamilyIndex,
		        .image               = m_SwapChainImages[imageIndex],
		        .subresourceRange    = imageSubresourceRange
		};

		m_PushConstantRay.clearColor     = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};
		m_PushConstantRay.lightPosition  = glm::vec3{10.f, 15.f, 8.f};
		m_PushConstantRay.lightIntensity = 100.f;
		m_PushConstantRay.lightType      = 0;
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RayTracingPipeline);

		VkDescriptorSet descSet{m_pParentDevice->GetDescriptorSet()};
		vkCmdBindDescriptorSets(
		        commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_PipelineLayout, 0, 1, &descSet, 0, nullptr
		);
		vkCmdPushConstants(
		        commandBuffer, m_PipelineLayout,
		        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0,
		        sizeof(PushConstantRay), &m_PushConstantRay
		);

		roing::vk::vkCmdTraceRaysKHR(
		        m_pParentDevice->GetHandle(), commandBuffer, &m_RgenRegion, &m_MissRegion, &m_HitRegion, &m_CallRegion,
		        window.GetWidth(), window.GetHeight(), 1
		);

		vkCmdPipelineBarrier(
		        commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
		        nullptr, 1, &fromPresentToClear
		);

		VkClearValue          clearValue{{{0.0f, 1.0f, 0.0f, 1.0f}}};
		VkRenderPassBeginInfo postRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		postRenderPassBeginInfo.clearValueCount = 1;
		postRenderPassBeginInfo.pClearValues    = &clearValue;
		postRenderPassBeginInfo.framebuffer     = m_SwapChainFramebuffers[imageIndex];
		postRenderPassBeginInfo.renderPass      = m_pParentDevice->m_RenderPass;
		postRenderPassBeginInfo.renderArea      = {{0, 0}, GetExtent().width, GetExtent().height};

		vkCmdBeginRenderPass(commandBuffer, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		auto aspectRatio{static_cast<float>(GetExtent().width) / static_cast<float>(GetExtent().height)};
		vkCmdPushConstants(
		        commandBuffer, device.m_PostPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &aspectRatio
		);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.m_PostPipeline);
		vkCmdBindDescriptorSets(
		        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.m_PostPipelineLayout, 0, 1,
		        &device.m_PostDescriptorSet, 0, nullptr
		);

		VkViewport viewport{};
		viewport.x        = 0.0f;
		viewport.y        = 0.0f;
		viewport.width    = static_cast<float>(GetExtent().width);
		viewport.height   = static_cast<float>(GetExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = m_SwapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		vkCmdEndRenderPass(commandBuffer);

		vkEndCommandBuffer(commandBuffer);
	}

	void Swapchain::CreateCommandBuffers() {
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool        = m_pParentDevice->GetCommandPoolHandle();
		allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = m_CommandBuffers.size();

		if (const VkResult result{
		            vkAllocateCommandBuffers(m_pParentDevice->GetHandle(), &allocateInfo, m_CommandBuffers.data())
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to allocate command buffers: "} + string_VkResult(result)};
		}
	}

	void Swapchain::CreateSyncObjects() {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i{0}; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			if (const VkResult result{vkCreateSemaphore(
			            m_pParentDevice->GetHandle(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]
			    )};
			    result != VK_SUCCESS) {
				throw std::runtime_error{
				        std::string{"Failed to create image available semaphore: "} + string_VkResult(result)
				};
			}

			if (const VkResult result{vkCreateSemaphore(
			            m_pParentDevice->GetHandle(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]
			    )};
			    result != VK_SUCCESS) {
				throw std::runtime_error{
				        std::string{"Failed to create render finished semaphore: "} + string_VkResult(result)
				};
			}

			if (const VkResult result{
			            vkCreateFence(m_pParentDevice->GetHandle(), &fenceInfo, nullptr, &m_InFlightFences[i])
			    };
			    result != VK_SUCCESS) {
				throw std::runtime_error{std::string{"Failed to create fence: "} + string_VkResult(result)};
			}
		}
	}

	bool Swapchain::DrawFrame(const Device &device, roing::vk::Window &window, const std::vector<Model> &models) {
		vkWaitForFences(m_pParentDevice->GetHandle(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex{};
		if (const VkResult result{vkAcquireNextImageKHR(
		            m_pParentDevice->GetHandle(), m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame],
		            VK_NULL_HANDLE, &imageIndex
		    )};
		    result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				return true;
			}

			if (result != VK_SUBOPTIMAL_KHR) {
				throw std::runtime_error{
				        std::string{"Failed to acquire swap chain image from swapchain: "} + string_VkResult(result)
				};
			}
		}

		vkResetFences(m_pParentDevice->GetHandle(), 1, &m_InFlightFences[m_CurrentFrame]);

		vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

		RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex, models, window, device);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		std::array<VkSemaphore, 1>          waitSemaphores{m_ImageAvailableSemaphores[m_CurrentFrame]};
		std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

		submitInfo.waitSemaphoreCount = waitSemaphores.size();
		submitInfo.pWaitSemaphores    = waitSemaphores.data();
		submitInfo.pWaitDstStageMask  = waitStages.data();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers    = &m_CommandBuffers[m_CurrentFrame];

		std::array<VkSemaphore, 1> signalSemaphores{m_RenderFinishedSemaphores[m_CurrentFrame]};
		submitInfo.signalSemaphoreCount = signalSemaphores.size();
		submitInfo.pSignalSemaphores    = signalSemaphores.data();

		m_pParentDevice->QueueSubmit(1, &submitInfo, m_InFlightFences[m_CurrentFrame]);

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = signalSemaphores.size();
		presentInfo.pWaitSemaphores    = signalSemaphores.data();

		std::array<VkSwapchainKHR, 1> swapChains{m_SwapChain};
		presentInfo.swapchainCount = swapChains.size();
		presentInfo.pSwapchains    = swapChains.data();
		presentInfo.pImageIndices  = &imageIndex;
		presentInfo.pResults       = nullptr;

		m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		return m_pParentDevice->QueuePresent(&presentInfo, window);
	}

	void Swapchain::CreateImageViews() {
		m_SwapChainImageViews.resize(m_SwapChainImages.size());

		for (size_t i{0}; i < m_SwapChainImages.size(); ++i) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image                           = m_SwapChainImages[i];
			createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format                          = m_SwapChainImageFormat;
			createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel   = 0;
			createInfo.subresourceRange.levelCount     = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount     = 1;

			if (const VkResult result{
			            vkCreateImageView(m_pParentDevice->GetHandle(), &createInfo, nullptr, &m_SwapChainImageViews[i])
			    };
			    result != VK_SUCCESS) {
				throw std::runtime_error{std::string{"Failed to create image view: "} + string_VkResult(result)};
			}
		}
	}

	void Swapchain::CreateRenderPass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format         = m_SwapChainImageFormat;
		colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments    = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments    = &colorAttachment;
		renderPassCreateInfo.subpassCount    = 1;
		renderPassCreateInfo.pSubpasses      = &subpassDescription;

		VkSubpassDependency dependency{};
		dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass    = 0;
		dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies   = &dependency;

		if (const VkResult result{
		            vkCreateRenderPass(m_pParentDevice->GetHandle(), &renderPassCreateInfo, nullptr, &m_RenderPass)
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create render pass: "} + string_VkResult(result)};
		}
	}

	void Swapchain::CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout) {
		auto rgenShaderCode{utils::ReadFile("shaders/raytrace.rgen.spv")};

		enum StageIndices {
			eRaygen,
			eMiss,
			eClosestHit,
			eShaderGroupCount,
		};

		std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
		VkPipelineShaderStageCreateInfo stageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stageCreateInfo.pName = "main";

		stageCreateInfo.module = CreateShaderModule(std::move(rgenShaderCode));
		stageCreateInfo.stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		stages[eRaygen]        = stageCreateInfo;

		auto rmissShaderCode{utils::ReadFile("shaders/raytrace.rmiss.spv")};
		stageCreateInfo.module = CreateShaderModule(std::move(rmissShaderCode));
		stageCreateInfo.stage  = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[eMiss]          = stageCreateInfo;

		auto rchitShaderCode{utils::ReadFile("shaders/raytrace.rchit.spv")};
		stageCreateInfo.module = CreateShaderModule(std::move(rchitShaderCode));
		stageCreateInfo.stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		stages[eClosestHit]    = stageCreateInfo;

		// Shader groups
		VkRayTracingShaderGroupCreateInfoKHR groupCreateInfo{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR
		};
		groupCreateInfo.anyHitShader       = VK_SHADER_UNUSED_KHR;
		groupCreateInfo.closestHitShader   = VK_SHADER_UNUSED_KHR;
		groupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

		groupCreateInfo.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		groupCreateInfo.generalShader = eRaygen;
		m_ShaderGroups.push_back(groupCreateInfo);

		groupCreateInfo.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		groupCreateInfo.generalShader = eMiss;
		m_ShaderGroups.push_back(groupCreateInfo);

		groupCreateInfo.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		groupCreateInfo.generalShader    = VK_SHADER_UNUSED_KHR;
		groupCreateInfo.closestHitShader = eClosestHit;
		m_ShaderGroups.push_back(groupCreateInfo);


		VkPushConstantRange pushConstantRange{
		        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0,
		        sizeof(PushConstantRay)
		};

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstantRange;

		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts    = &descriptorSetLayout;

		vkCreatePipelineLayout(m_pParentDevice->GetHandle(), &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout);

		VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
		rayPipelineInfo.stageCount = stages.size();
		rayPipelineInfo.pStages    = stages.data();

		rayPipelineInfo.groupCount = m_ShaderGroups.size();
		rayPipelineInfo.pGroups    = m_ShaderGroups.data();

		rayPipelineInfo.maxPipelineRayRecursionDepth = 1;// ray depth
		rayPipelineInfo.layout                       = m_PipelineLayout;

		roing::vk::vkCreateRayTracingPipelinesKHR(
		        m_pParentDevice->GetHandle(), {}, {}, 1, &rayPipelineInfo, nullptr, &m_RayTracingPipeline
		);

		for (auto &stage: stages) { vkDestroyShaderModule(m_pParentDevice->GetHandle(), stage.module, nullptr); }
	}

	VkShaderModule Swapchain::CreateShaderModule(std::vector<char> &&code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

		VkShaderModule shaderModule{};
		if (const VkResult result{
		            vkCreateShaderModule(m_pParentDevice->GetHandle(), &createInfo, nullptr, &shaderModule)
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create shader module: "} + string_VkResult(result)};
		}

		return shaderModule;
	}

	VkImageView Swapchain::GetCurrentImageView() const noexcept {
		return m_SwapChainImageViews[m_CurrentFrame];
	}

	void Swapchain::Create(
	        Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice,
	        VkDescriptorSetLayout descriptorSetLayout
	) {
		Init(pParentDevice, window, surface, physicalDevice);

		CreateImageViews();
		//		CreateRenderPass();
		CreateGraphicsPipeline(descriptorSetLayout);
		//		CreateFramebuffers();
		CreateCommandBuffers();
		CreateSyncObjects();
	}

	void Swapchain::Recreate(
	        roing::vk::Device *pParentDevice, const roing::vk::Window &window, const roing::vk::Surface &surface,
	        VkPhysicalDevice physicalDevice
	) {
		CleanupOnlySwapchain();

		Init(pParentDevice, window, surface, physicalDevice);

		CreateImageViews();
		CreateFramebuffers();
	}

	void Swapchain::CreateRtShaderBindingTable(VkPhysicalDevice physicalDevice) {
		uint32_t missCount{1};
		uint32_t hitCount{1};
		auto     handleCount{1 + missCount + hitCount};
		uint32_t handleSize{m_RtProperties.shaderGroupHandleSize};

		uint32_t handleSizeAligned{AlignUp(handleSize, m_RtProperties.shaderGroupHandleAlignment)};

		m_RgenRegion.stride = AlignUp(handleSizeAligned, m_RtProperties.shaderGroupBaseAlignment);
		m_RgenRegion.size   = m_RgenRegion.stride;
		m_MissRegion.stride = handleSizeAligned;
		m_MissRegion.size   = AlignUp(missCount * handleSizeAligned, m_RtProperties.shaderGroupBaseAlignment);
		m_HitRegion.stride  = handleSizeAligned;
		m_HitRegion.size    = AlignUp(hitCount * handleSizeAligned, m_RtProperties.shaderGroupBaseAlignment);

		uint32_t             dataSize{handleCount * handleSize};
		std::vector<uint8_t> handles(dataSize);
		auto                 result{roing::vk::vkGetRayTracingShaderGroupHandlesKHR(
                m_pParentDevice->GetHandle(), m_RayTracingPipeline, 0, handleCount, dataSize, handles.data()
        )};
		assert(result == VK_SUCCESS);

		const VkDeviceSize sbtSize{m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size + m_CallRegion.size};
		m_SBTBuffer = m_pParentDevice->CreateBuffer(
		        physicalDevice, sbtSize,
		        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		const VkDeviceAddress sbtAddress{m_SBTBuffer.GetDeviceAddress()};
		m_RgenRegion.deviceAddress = sbtAddress;
		m_MissRegion.deviceAddress = sbtAddress + m_RgenRegion.size;
		m_HitRegion.deviceAddress  = sbtAddress + m_RgenRegion.size + m_MissRegion.size;

		auto getHandle = [&](uint32_t i) { return handles.data() + i * handleSize; };

		uint8_t *pSBTBuffer{};
		if (vkMapMemory(
		            m_pParentDevice->GetHandle(), m_SBTBuffer.GetMemoryHandle(), 0, VK_WHOLE_SIZE, 0,
		            reinterpret_cast<void **>(&pSBTBuffer)
		    ) != VK_SUCCESS) {
			throw std::runtime_error{"Failed to map SBT buffer to memory."};
		}

		uint8_t *pData{};
		uint32_t handleIdx{0};

		pData = pSBTBuffer;
		memcpy(pData, getHandle(handleIdx++), handleSize);

		pData = pSBTBuffer + m_RgenRegion.size;
		for (uint32_t c{0}; c < missCount; ++c) {
			memcpy(pData, getHandle(handleIdx++), handleSize);
			pData += m_MissRegion.stride;
		}

		pData = pSBTBuffer + m_RgenRegion.size + m_MissRegion.size;
		for (uint32_t c{0}; c < hitCount; ++c) {
			memcpy(pData, getHandle(handleIdx++), handleSize);
			pData += m_HitRegion.stride;
		}

		vkUnmapMemory(m_pParentDevice->GetHandle(), m_SBTBuffer.GetMemoryHandle());
	}

	VkImageView Swapchain::GetOutputImageView() const noexcept {
		return m_OutImageView;
	}

	Texture &Swapchain::GetOutputTexture() noexcept {
		return m_OutImage;
	}

	void Swapchain::CmdBarrierImageLayout(
	        VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	        const VkImageSubresourceRange &subresourceRange
	) {
		VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imageMemoryBarrier.oldLayout        = oldLayout;
		imageMemoryBarrier.newLayout        = newLayout;
		imageMemoryBarrier.image            = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;
		imageMemoryBarrier.srcAccessMask    = AccessFlagsForImageLayout(oldLayout);
		imageMemoryBarrier.dstAccessMask    = AccessFlagsForImageLayout(newLayout);

		const VkPipelineStageFlags srcStageMask{PipelineStageForLayout(oldLayout)};
		const VkPipelineStageFlags dstStageMask{PipelineStageForLayout(newLayout)};
		vkCmdPipelineBarrier(
		        commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier
		);
	}

	void Swapchain::CmdBarrierImageLayout(
	        VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	        VkImageAspectFlags imageAspectFlags
	) {
		VkImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask     = imageAspectFlags;
		subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
		subresourceRange.baseMipLevel   = 0;
		subresourceRange.baseArrayLayer = 0;
		CmdBarrierImageLayout(commandBuffer, image, oldLayout, newLayout, subresourceRange);
	}

	VkAccessFlags Swapchain::AccessFlagsForImageLayout(VkImageLayout layout) {
		switch (layout) {
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return VK_ACCESS_HOST_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_ACCESS_TRANSFER_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_ACCESS_TRANSFER_READ_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_ACCESS_SHADER_READ_BIT;
			default:
				return VkAccessFlags();
		}
	}

	VkPipelineStageFlags Swapchain::PipelineStageForLayout(VkImageLayout layout) {
		switch (layout) {
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return VK_PIPELINE_STAGE_HOST_BIT;
			case VK_IMAGE_LAYOUT_UNDEFINED:
				return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			default:
				return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}
	}

	VkExtent2D Swapchain::GetExtent() const noexcept {
		return m_SwapChainExtent;
	}

	VkFormat Swapchain::GetFormat() const noexcept {
		return m_SwapChainImageFormat;
	}

	VkFramebuffer Swapchain::GetFramebuffer() const noexcept {
		return m_SwapChainFramebuffers[m_CurrentFrame];
	}

	VkCommandBuffer Swapchain::GetCommandBuffer() const noexcept {
		return m_CommandBuffers[m_CurrentFrame];
	}

}// namespace roing::vk
