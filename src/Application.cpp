#include "Application.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

void Application::Run() {
	InitWindow();
	InitVulkan();
	MainLoop();
	Cleanup();
}

void Application::InitWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE.data(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetFramebufferSizeCallback(m_pWindow, FramebufferResizeCallback);
}

void Application::InitVulkan() {
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFramebuffers();
	CreateCommandPool();
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateCommandBuffers();
	CreateSyncObjects();
}

void Application::MainLoop() {
	while (!glfwWindowShouldClose(m_pWindow)) {
		glfwPollEvents();
		DrawFrame();
	}

	vkDeviceWaitIdle(m_Device);
}

void Application::DrawFrame() {
	vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex{};
	if (const VkResult result{vkAcquireNextImageKHR(
	            m_Device, m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE,
	            &imageIndex
	    )};
	    result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapChain();
			return;
		}

		if (result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error{
			        std::string{"Failed to acquire swap chain image from swapchain: "} + string_VkResult(result)
			};
		}
	}

	vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

	vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

	RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);

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

	if (const VkResult result{vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame])};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to submit draw command buffer: "} + string_VkResult(result)};
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = signalSemaphores.size();
	presentInfo.pWaitSemaphores    = signalSemaphores.data();

	std::array<VkSwapchainKHR, 1> swapChains{m_SwapChain};
	presentInfo.swapchainCount = swapChains.size();
	presentInfo.pSwapchains    = swapChains.data();
	presentInfo.pImageIndices  = &imageIndex;
	presentInfo.pResults       = nullptr;

	if (const VkResult result{vkQueuePresentKHR(m_PresentQueue, &presentInfo)}; result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
			m_FramebufferResized = false;
			RecreateSwapChain();
		} else {
			throw std::runtime_error{std::string{"Failed to present swap chain image: "} + string_VkResult(result)};
		}
	}

	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::Cleanup() {
	for (size_t i{0}; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

	CleanupSwapChain();

	vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
	vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);

	vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
	vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);

	vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);

	vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

	vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

	vkDestroyDevice(m_Device, nullptr);

	if (ENABLE_VALIDATION_LAYERS) {
		DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

	vkDestroyInstance(m_Instance, nullptr);

	glfwDestroyWindow(m_pWindow);

	glfwTerminate();
}

void Application::CreateInstance() {
	if (ENABLE_VALIDATION_LAYERS && !CheckValidationLayerSupport()) {
		throw std::runtime_error{"Validation layers requested but not available"};
	}

	VkApplicationInfo appInfo{};
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName   = "Portal 2 Ray Traced";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName        = "Roingus Engine";
	appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion         = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	uint32_t     glfwExtensionCount{0};
	const char **glfwExtensions{glfwGetRequiredInstanceExtensions(&glfwExtensionCount)};

	createInfo.enabledExtensionCount   = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
	createInfo.enabledLayerCount       = 0;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (ENABLE_VALIDATION_LAYERS) {
		createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;
	} else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	PrintAvailableInstanceExtensions();

	const auto extensions{GetRequiredExtensions()};
	createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (const VkResult result{vkCreateInstance(&createInfo, nullptr, &m_Instance)}; result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create instance: "} + string_VkResult(result)};
	}
}

void Application::FramebufferResizeCallback(GLFWwindow *pApplication, int width, int height) {
	auto appPtr{reinterpret_cast<Application *>(pApplication)};
	appPtr->m_FramebufferResized = true;
}

std::vector<char> Application::ReadFile(std::string_view fileName) {
	std::ifstream file{fileName.data(), std::ios::ate | std::ios::binary};

	if (!file.is_open()) {
		throw std::runtime_error{"Failed to open file: "};
	}

	size_t            fileSize{static_cast<size_t>(file.tellg())};
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

	file.close();

	return buffer;
}

bool Application::CheckValidationLayerSupport() {
	uint32_t layerCount{};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const auto layerName: VALIDATION_LAYERS) {
		bool layerFound{false};

		for (const auto &layerProperties: availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}

	return true;
}

void Application::PrintAvailableInstanceExtensions() {
	uint32_t extensionCount{};
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
}

std::vector<const char *> Application::GetRequiredExtensions() {
	uint32_t     glfwExtensionCount{0};
	const char **glfwExtensions{glfwGetRequiredInstanceExtensions(&glfwExtensionCount)};

	std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (ENABLE_VALIDATION_LAYERS) {
		extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	extensions.insert(extensions.end(), INSTANCE_EXTENSIONS.cbegin(), INSTANCE_EXTENSIONS.cend());

	return extensions;
}

VkBool32 Application::DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData
) {
	std::ostream &ostream{messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? std::cerr : std::cout};

	ostream << "Validation layer: " << pCallbackData->pMessage << '\n';

	return VK_FALSE;
}

void Application::SetupDebugMessenger() {
	if (!ENABLE_VALIDATION_LAYERS)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (const VkResult result{CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to set up debug messenger: "} + string_VkResult(result)};
	}
}

void Application::PickPhysicalDevice() {
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error{"Failed to find GPUs with Vulkan support."};
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

	const auto firstSuitableDeviceIterator{std::find_if(devices.cbegin(), devices.cend(), [this](const auto &device) {
		return IsDeviceSuitable(device);
	})};
	if (firstSuitableDeviceIterator == devices.cend())
		throw std::runtime_error{"Failed to find a suitable GPU"};

	m_PhysicalDevice = *firstSuitableDeviceIterator;
}

void Application::CreateLogicalDevice() {
	QueueFamilyIndices indices{FindQueueFamilies(m_PhysicalDevice)};

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
	std::set<uint32_t> uniqueQueueFamilies{indices.graphicsFamily.value(), indices.presentFamily.value()};

	float queuePriority = 1.f;
	for (uint32_t queueFamily: uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount       = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.emplace_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = queueCreateInfos.size();
	createInfo.pQueueCreateInfos    = queueCreateInfos.data();
	createInfo.pEnabledFeatures     = &deviceFeatures;

	if (ENABLE_VALIDATION_LAYERS) {
		createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	createInfo.enabledExtensionCount   = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
	createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

	if (const VkResult result{vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create logical device: "} + string_VkResult(result)};
	}

	vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void Application::CreateSurface() {
	if (const VkResult result{glfwCreateWindowSurface(m_Instance, m_pWindow, nullptr, &m_Surface)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create window surface: "} + string_VkResult(result)};
	}
}

void Application::CreateSwapChain() {
	const SwapChainSupportDetails swapChainSupport{QuerySwapChainSupport(m_PhysicalDevice)};

	const VkSurfaceFormatKHR surfaceFormat{ChooseSwapSurfaceFormat(swapChainSupport.formats)};
	const VkPresentModeKHR   presentMode{ChooseSwapPresentMode(swapChainSupport.presentModes)};
	const VkExtent2D         extent{ChooseSwapExtent(swapChainSupport.capabilities)};

	uint32_t imageCount{swapChainSupport.capabilities.minImageCount + 1};

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.minImageCount    = imageCount;
	createInfo.surface          = m_Surface;
	createInfo.imageFormat      = surfaceFormat.format;
	createInfo.imageColorSpace  = surfaceFormat.colorSpace;
	createInfo.imageExtent      = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices      indices{FindQueueFamilies(m_PhysicalDevice)};
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

	if (const VkResult result{vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create swap chain: "} + string_VkResult(result)};
	}

	vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
	m_SwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

	m_SwapChainImageFormat = surfaceFormat.format;
	m_SwapChainExtent      = extent;
}

void Application::CreateImageViews() {
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

		if (const VkResult result{vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapChainImageViews[i])};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create image view: "} + string_VkResult(result)};
		}
	}
}

void Application::CreateGraphicsPipeline() {
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

	const auto bindingDescription{Vertex::GetBindingDescription()};
	const auto attributeDescriptions{Vertex::GetAttributeDescriptions()};

	vertexInputInfo.vertexBindingDescriptionCount   = 1;
	vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

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
	colorBlendAttachment.colorWriteMask =
	        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

	if (const VkResult result{vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout)};
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
	            m_Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_GraphicsPipeline
	    )};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create graphics pipeline: "} + string_VkResult(result)};
	}

	vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
	vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
}

void Application::CreateRenderPass() {
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

	if (const VkResult result{vkCreateRenderPass(m_Device, &renderPassCreateInfo, nullptr, &m_RenderPass)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create render pass: "} + string_VkResult(result)};
	}
}

void Application::CreateFramebuffers() {
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

		if (const VkResult result{
		            vkCreateFramebuffer(m_Device, &framebufferCreateInfo, nullptr, &m_SwapChainFramebuffers[i])
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create framebuffer: "} + string_VkResult(result)};
		}
	}
}

void Application::CreateCommandPool() {
	QueueFamilyIndices queueFamilyIndices{FindQueueFamilies(m_PhysicalDevice)};

	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (const VkResult result{vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create command pool: "} + string_VkResult(result)};
	}
}

void Application::CreateCommandBuffers() {
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool        = m_CommandPool;
	allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = m_CommandBuffers.size();

	if (const VkResult result{vkAllocateCommandBuffers(m_Device, &allocateInfo, m_CommandBuffers.data())};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to allocate command buffers: "} + string_VkResult(result)};
	}
}

void Application::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
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

	std::array<VkBuffer, 1>     vertexBuffers{m_VertexBuffer};
	std::array<VkDeviceSize, 1> offsets{0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers.data(), offsets.data());

	vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdDrawIndexed(commandBuffer, INDICES.size(), 1, 0, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	if (const VkResult result{vkEndCommandBuffer(commandBuffer)}; result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to record command buffer: "} + string_VkResult(result)};
	}
}

void Application::CreateSyncObjects() {
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i{0}; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		if (const VkResult result{vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i])};
		    result != VK_SUCCESS) {
			throw std::runtime_error{
			        std::string{"Failed to create image available semaphore: "} + string_VkResult(result)
			};
		}

		if (const VkResult result{vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i])};
		    result != VK_SUCCESS) {
			throw std::runtime_error{
			        std::string{"Failed to create render finished semaphore: "} + string_VkResult(result)
			};
		}

		if (const VkResult result{vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i])};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create fence: "} + string_VkResult(result)};
		}
	}
}

void Application::RecreateSwapChain() {
	int width, height;
	glfwGetFramebufferSize(m_pWindow, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(m_pWindow, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_Device);

	CleanupSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateFramebuffers();
}

void Application::CleanupSwapChain() {
	for (auto framebuffer: m_SwapChainFramebuffers) { vkDestroyFramebuffer(m_Device, framebuffer, nullptr); }

	for (auto imageView: m_SwapChainImageViews) { vkDestroyImageView(m_Device, imageView, nullptr); }

	vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
}

void Application::CreateVertexBuffer() {
	VkDeviceSize bufferSize{sizeof(Vertex) * VERTICES.size()};

	VkBuffer       stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(
	        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
	        stagingBufferMemory
	);

	void *data;
	vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, VERTICES.data(), bufferSize);
	vkUnmapMemory(m_Device, stagingBufferMemory);

	CreateBuffer(
	        bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory
	);

	CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

	vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void Application::CreateIndexBuffer() {
	VkDeviceSize bufferSize{sizeof(INDICES[0]) * INDICES.size()};

	VkBuffer       stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(
	        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
	        stagingBufferMemory
	);

	void *data;
	vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, INDICES.data(), bufferSize);
	vkUnmapMemory(m_Device, stagingBufferMemory);

	CreateBuffer(
	        bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory
	);

	CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

	vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void Application::CreateBuffer(
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer,
        VkDeviceMemory &bufferMemory
) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size        = size;
	bufferInfo.usage       = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (const VkResult result{vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer)}; result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create buffer: "} + string_VkResult(result)};
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(m_Device, buffer, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize  = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);

	if (const VkResult result{vkAllocateMemory(m_Device, &memoryAllocateInfo, nullptr, &bufferMemory)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to allocate memory: "} + string_VkResult(result)};
	}

	vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
}

void Application::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool        = m_CommandPool;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_Device, &allocateInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size      = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers    = &commandBuffer;

	vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_GraphicsQueue);

	vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
}

uint32_t Application::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);

	//TODO: prefer vram resource
	for (uint32_t i{0}; i < memoryProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error{"Failed to find a suitable memory type"};
}

VkShaderModule Application::CreateShaderModule(std::vector<char> &&code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule{};
	if (const VkResult result{vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule)};
	    result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create shader module: "} + string_VkResult(result)};
	}

	return shaderModule;
}

QueueFamilyIndices Application::FindQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount{0};
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	for (int idx{}; idx < queueFamilies.size(); ++idx) {
		VkBool32 presentSupport{false};
		vkGetPhysicalDeviceSurfaceSupportKHR(device, idx, m_Surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = idx;
		}

		if (queueFamilies[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = idx;
		}

		if (indices.IsComplete()) {
			break;
		}
	}

	return indices;
}

VkSurfaceFormatKHR Application::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
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

VkPresentModeKHR Application::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
	const auto presentModeIterator{std::find_if(
	        availablePresentModes.cbegin(), availablePresentModes.cend(),
	        [](const VkPresentModeKHR &presentMode) { return presentMode == VK_PRESENT_MODE_MAILBOX_KHR; }
	)};

	if (presentModeIterator != availablePresentModes.cend()) {
		return *presentModeIterator;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(m_pWindow, &width, &height);

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

SwapChainSupportDetails Application::QuerySwapChainSupport(VkPhysicalDevice device) {
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
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

bool Application::IsDeviceSuitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures{};
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	const bool extensionsSupported{CheckDeviceExtensionSupport(device)};

	bool swapChainAdequate{false};
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport{QuerySwapChainSupport(device)};
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader &&
	       FindQueueFamilies(device).IsComplete() && CheckDeviceExtensionSupport(device) && swapChainAdequate;
}

VkResult Application::CreateDebugUtilsMessengerEXT(
        VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
        const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger
) {
	const auto func{reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
	        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
	)};

	if (func == nullptr)
		return VK_ERROR_EXTENSION_NOT_PRESENT;

	return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void Application::DestroyDebugUtilsMessengerEXT(
        VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator
) {
	const auto func{reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
	        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
	)};

	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void Application::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
	createInfo                 = {};
	createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData       = nullptr;
}

bool Application::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::cout << "Available instance extensions:\n";

	std::transform(
	        availableExtensions.cbegin(), availableExtensions.cend(),
	        std::ostream_iterator<const char *>{std::cout, "\n"},
	        [](const VkExtensionProperties &ext) { return ext.extensionName; }
	);
	std::cout << '\n';

	std::set<std::string_view> requiredExtensions(DEVICE_EXTENSIONS.cbegin(), DEVICE_EXTENSIONS.cend());

	for (const auto &extension: availableExtensions) { requiredExtensions.erase(extension.extensionName); }

	return requiredExtensions.empty();
}

constexpr VkVertexInputBindingDescription Vertex::GetBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding   = 0;
	bindingDescription.stride    = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

constexpr std::array<VkVertexInputAttributeDescription, 2> Vertex::GetAttributeDescriptions() {
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
	attributeDescriptions[0].binding  = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[0].offset   = offsetof(Vertex, pos);

	attributeDescriptions[1].binding  = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset   = offsetof(Vertex, color);

	return attributeDescriptions;
}
