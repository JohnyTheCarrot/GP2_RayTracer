#include "Application.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE.data(), nullptr, nullptr);
}

void Application::InitVulkan() {
	CreateInstance();
	SetupDebugMessenger();
	PickPhysicalDevice();
}

void Application::MainLoop() {
	while (!glfwWindowShouldClose(m_pWindow)) { glfwPollEvents(); }
}

void Application::Cleanup() {
	if (ENABLE_VALIDATION_LAYERS) {
		DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
	}

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
	appInfo.apiVersion         = VK_API_VERSION_1_0;

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

	PrintAvailableExtensions();

	const auto extensions{GetRequiredExtensions()};
	createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (const VkResult result{vkCreateInstance(&createInfo, nullptr, &m_Instance)}; result != VK_SUCCESS) {
		throw std::runtime_error{std::string{"Failed to create instance: "} + string_VkResult(result)};
	}
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

void Application::PrintAvailableExtensions() {
	uint32_t extensionCount{};
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	std::cout << "Available extensions:\n";

	std::transform(
	        extensions.cbegin(), extensions.cend(), std::ostream_iterator<const char *>{std::cout, "\n"},
	        [](const VkExtensionProperties &ext) { return ext.extensionName; }
	);
}

std::vector<const char *> Application::GetRequiredExtensions() {
	uint32_t     glfwExtensionCount{0};
	const char **glfwExtensions{glfwGetRequiredInstanceExtensions(&glfwExtensionCount)};

	std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (ENABLE_VALIDATION_LAYERS) {
		extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

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

	const auto firstSuitableDeviceIterator{std::find_if(devices.cbegin(), devices.cend(), IsDeviceSuitable)};
	if (firstSuitableDeviceIterator == devices.cend())
		throw std::runtime_error{"Failed to find a suitable GPU"};

	m_PhysicalDevice = *firstSuitableDeviceIterator;
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

bool Application::IsDeviceSuitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures{};
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader &&
	       FindQueueFamilies(device).IsComplete();
}

QueueFamilyIndices Application::FindQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount{0};
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	for (int idx{}; idx < queueFamilies.size(); ++idx) {
		if (queueFamilies[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = idx;
		}

		if (indices.IsComplete()) {
			break;
		}
	}

	return indices;
}
