#include "Swapchain.h"
#include "../Model.h"
#include "../utils/read_file.h"
#include "Device.h"
#include "PhysicalDeviceUtils.h"
#include "QueueFamilyIndices.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Swapchain::~Swapchain() {
		if (m_pParentDevice == nullptr)
			return;

		for (size_t i{0}; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vkDestroySemaphore(m_pParentDevice->GetHandle(), m_ImageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(m_pParentDevice->GetHandle(), m_RenderFinishedSemaphores[i], nullptr);
			vkDestroyFence(m_pParentDevice->GetHandle(), m_InFlightFences[i], nullptr);
		}

		for (auto framebuffer: m_SwapChainFramebuffers) {
			vkDestroyFramebuffer(m_pParentDevice->GetHandle(), framebuffer, nullptr);
		}

		for (auto imageView: m_SwapChainImageViews) {
			vkDestroyImageView(m_pParentDevice->GetHandle(), imageView, nullptr);
		}

		vkDestroyPipeline(m_pParentDevice->GetHandle(), m_GraphicsPipeline, nullptr);

		vkDestroyPipelineLayout(m_pParentDevice->GetHandle(), m_PipelineLayout, nullptr);

		vkDestroySwapchainKHR(m_pParentDevice->GetHandle(), m_SwapChain, nullptr);

		vkDestroyRenderPass(m_pParentDevice->GetHandle(), m_RenderPass, nullptr);
	}

	Swapchain::Swapchain(
	        Device *pParentDevice, const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice
	)
	    : m_pParentDevice{pParentDevice} {
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
		createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
	}

	VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
		const auto formatIterator{std::find_if(
		        availableFormats.cbegin(), availableFormats.cend(),
		        [](const VkSurfaceFormatKHR &format) {
			        return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
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
			framebufferCreateInfo.renderPass      = m_RenderPass;
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
	        VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<Model> &models
	) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags            = 0;
		beginInfo.pInheritanceInfo = nullptr;

		if (const VkResult result{vkBeginCommandBuffer(commandBuffer, &beginInfo)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to begin command buffer: "} + string_VkResult(result)};
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass        = m_RenderPass;
		renderPassInfo.framebuffer       = m_SwapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = m_SwapChainExtent;

		VkClearValue clearValue{{{0.0f, 0.0f, 0.0f, 1.0f}}};
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues    = &clearValue;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		// TODO: this is repeated
		VkViewport viewport{};
		viewport.x        = 0.f;
		viewport.y        = 0.f;
		viewport.width    = static_cast<float>(m_SwapChainExtent.width);
		viewport.height   = static_cast<float>(m_SwapChainExtent.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = m_SwapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		std::for_each(models.begin(), models.end(), [&](const auto &model) { model.DrawModel(commandBuffer); });

		vkCmdEndRenderPass(commandBuffer);

		if (const VkResult result{vkEndCommandBuffer(commandBuffer)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to record command buffer: "} + string_VkResult(result)};
		}
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

	bool Swapchain::DrawFrame(roing::vk::Window &window, const std::vector<Model> &models) {
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

		RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex, models);

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

	void Swapchain::CreateGraphicsPipeline() {
		auto vertShaderCode{ReadFile("shaders/shader.vert.spv")};
		auto fragShaderCode{ReadFile("shaders/shader.frag.spv")};

		VkShaderModule vertShaderModule{CreateShaderModule(std::move(vertShaderCode))};
		VkShaderModule fragShaderModule{CreateShaderModule(std::move(fragShaderCode))};

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName  = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName  = "main";

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo, fragShaderStageInfo};

		std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
		dynamicStateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = dynamicStates.size();
		dynamicStateInfo.pDynamicStates    = dynamicStates.data();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		vertexInputInfo.vertexBindingDescriptionCount   = 1;
		vertexInputInfo.pVertexBindingDescriptions      = &VERTEX_BINDING_DESCRIPTION;
		vertexInputInfo.vertexAttributeDescriptionCount = VERTEX_ATTRIBUTE_DESCRIPTIONS.size();
		vertexInputInfo.pVertexAttributeDescriptions    = VERTEX_ATTRIBUTE_DESCRIPTIONS.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x        = 0.0f;
		viewport.y        = 0.0f;
		viewport.width    = static_cast<float>(m_SwapChainExtent.width);
		viewport.height   = static_cast<float>(m_SwapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = m_SwapChainExtent;

		VkPipelineViewportStateCreateInfo viewportStateInfo{};
		viewportStateInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.scissorCount  = 1;

		VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
		rasterizerInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerInfo.depthClampEnable        = VK_FALSE;
		rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerInfo.polygonMode             = VK_POLYGON_MODE_FILL;
		rasterizerInfo.lineWidth               = 1.0f;
		rasterizerInfo.cullMode                = VK_CULL_MODE_BACK_BIT;
		rasterizerInfo.frontFace               = VK_FRONT_FACE_CLOCKWISE;
		rasterizerInfo.depthBiasEnable         = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampleInfo{};
		multisampleInfo.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleInfo.sampleShadingEnable  = VK_FALSE;
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable         = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable     = VK_FALSE;
		colorBlending.logicOp           = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount   = 1;
		colorBlending.pAttachments      = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		if (const VkResult result{vkCreatePipelineLayout(
		            m_pParentDevice->GetHandle(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout
		    )};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create pipeline layout: "} + string_VkResult(result)};
		}

		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount          = shaderStages.size();
		pipelineCreateInfo.pStages             = shaderStages.data();
		pipelineCreateInfo.pVertexInputState   = &vertexInputInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineCreateInfo.pViewportState      = &viewportStateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
		pipelineCreateInfo.pMultisampleState   = &multisampleInfo;
		pipelineCreateInfo.pDepthStencilState  = nullptr;
		pipelineCreateInfo.pColorBlendState    = &colorBlending;
		pipelineCreateInfo.pDynamicState       = &dynamicStateInfo;
		pipelineCreateInfo.layout              = m_PipelineLayout;
		pipelineCreateInfo.renderPass          = m_RenderPass;
		pipelineCreateInfo.subpass             = 0;

		// Creating new pipeline from existing one!
		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex  = -1;

		if (const VkResult result{vkCreateGraphicsPipelines(
		            m_pParentDevice->GetHandle(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_GraphicsPipeline
		    )};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create graphics pipeline: "} + string_VkResult(result)};
		}

		vkDestroyShaderModule(m_pParentDevice->GetHandle(), vertShaderModule, nullptr);
		vkDestroyShaderModule(m_pParentDevice->GetHandle(), fragShaderModule, nullptr);
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
}// namespace roing::vk
