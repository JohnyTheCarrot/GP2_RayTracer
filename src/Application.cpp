#include "Application.h"
#include "src/vk/PhysicalDeviceUtils.h"
#include "tiny_obj_loader.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

namespace roing {
	void Application::Run() {
		InitWindow();
		InitVulkan();
		MainLoop();
		Cleanup();
	}

	void Application::InitWindow() {
		m_Window = {WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE};
	}

	void Application::InitVulkan() {
		m_Instance.Init();

		m_Surface = vk::Surface{m_Instance, m_Window};

		PickPhysicalDevice();

		m_Device = vk::Device{m_Surface, m_PhysicalDevice};

		vk::Swapchain &swapchain{m_Device.CreateSwapchain(m_Window, m_Surface, m_PhysicalDevice)};

		swapchain.CreateImageViews();
		swapchain.CreateRenderPass();
		swapchain.CreateGraphicsPipeline();
		swapchain.CreateFramebuffers();
		m_Device.CreateCommandPool(m_Surface, m_PhysicalDevice);
		m_Models.emplace_back(m_PhysicalDevice, m_Device, VERTICES, INDICES);
		swapchain.CreateCommandBuffers();
		swapchain.CreateSyncObjects();

		InitRayTracing();
	}

	void Application::MainLoop() {
		while (!glfwWindowShouldClose(m_Window.Get())) {
			glfwPollEvents();
			if (m_Device.GetSwapchain().DrawFrame(m_Window, m_Models))
				m_Device.RecreateSwapchain(m_Window, m_Surface, m_PhysicalDevice);
		}

		vkDeviceWaitIdle(m_Device.GetHandle());
	}

	void Application::DrawFrame() {
	}

	void Application::Cleanup() {
		glfwTerminate();
	}

	void Application::InitRayTracing() {
		VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		properties2.pNext = &m_RtProperties;
		vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &properties2);
	}

	void Application::PickPhysicalDevice() {
		uint32_t deviceCount;
		vkEnumeratePhysicalDevices(m_Instance.Get(), &deviceCount, nullptr);

		if (deviceCount == 0) {
			throw std::runtime_error{"Failed to find GPUs with Vulkan support."};
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance.Get(), &deviceCount, devices.data());

		const auto firstSuitableDeviceIterator{std::find_if(
		        devices.cbegin(), devices.cend(), [this](const auto &device) { return IsDeviceSuitable(device); }
		)};
		if (firstSuitableDeviceIterator == devices.cend())
			throw std::runtime_error{"Failed to find a suitable GPU"};

		m_PhysicalDevice = *firstSuitableDeviceIterator;
	}

	VkShaderModule Application::CreateShaderModule(std::vector<char> &&code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

		VkShaderModule shaderModule{};
		if (const VkResult result{vkCreateShaderModule(m_Device.GetHandle(), &createInfo, nullptr, &shaderModule)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create shader module: "} + string_VkResult(result)};
		}

		return shaderModule;
	}

	bool Application::IsDeviceSuitable(VkPhysicalDevice device) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures{};
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		const bool extensionsSupported{CheckDeviceExtensionSupport(device)};

		bool swapChainAdequate{false};
		if (extensionsSupported) {
			vk::SwapChainSupportDetails swapChainSupport{m_Surface.QuerySwapChainSupport(device)};
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader &&
		       vk::physical_device::FindQueueFamilies(m_Surface, device).IsComplete() &&
		       CheckDeviceExtensionSupport(device) && swapChainAdequate;
	}

	bool Application::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::cout << "Available device extensions:\n";

		std::transform(
		        availableExtensions.cbegin(), availableExtensions.cend(),
		        std::ostream_iterator<const char *>{std::cout, "\n"},
		        [](const VkExtensionProperties &ext) { return ext.extensionName; }
		);
		std::cout << '\n';

		std::set<std::string_view> requiredExtensions(
		        vk::Device::DEVICE_EXTENSIONS.cbegin(), vk::Device::DEVICE_EXTENSIONS.cend()
		);

		for (const auto &extension: availableExtensions) { requiredExtensions.erase(extension.extensionName); }

		return requiredExtensions.empty();
	}
}// namespace roing
