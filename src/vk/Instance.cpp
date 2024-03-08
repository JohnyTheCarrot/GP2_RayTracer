#include "Instance.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Instance::~Instance() noexcept {
#ifdef ENABLE_VALIDATION_LAYERS
		DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif

		vkDestroyInstance(m_Instance, nullptr);
	}

	void Instance::Init() {
#ifdef ENABLE_VALIDATION_LAYERS
		if (!CheckValidationLayerSupport()) {
			throw std::runtime_error{"Validation layers requested but not available"};
		}
#endif

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

#ifdef ENABLE_VALIDATION_LAYERS
		createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

		createInfo.pNext = &DEBUG_UTILS_MESSENGER_CREATE_INFO;
#else
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
#endif

		PrintAvailableInstanceExtensions();

		const auto extensions{GetRequiredExtensions()};
		createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (const VkResult result{vkCreateInstance(&createInfo, nullptr, &m_Instance)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create instance: "} + string_VkResult(result)};
		}

#ifdef ENABLE_VALIDATION_LAYERS
		SetupDebugMessenger();
#endif
	}

	VkBool32 Instance::DebugCallback(
	        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData
	) {
		std::ostream &ostream{
		        messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? std::cerr : std::cout
		};

		ostream << "Validation layer: " << pCallbackData->pMessage << '\n';

		return VK_FALSE;
	}

	void Instance::PrintAvailableInstanceExtensions() {
		uint32_t extensionCount{};
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::cout << "Available instance extensions:\n";

		std::transform(
		        extensions.cbegin(), extensions.cend(), std::ostream_iterator<const char *>{std::cout, "\n"},
		        [](const VkExtensionProperties &ext) { return ext.extensionName; }
		);
		std::cout << '\n';
	}

	std::vector<const char *> Instance::GetRequiredExtensions() {
		uint32_t     glfwExtensionCount{0};
		const char **glfwExtensions{glfwGetRequiredInstanceExtensions(&glfwExtensionCount)};

		if (glfwExtensions == nullptr) {
			const char *errorBuff{};
			const int   error{glfwGetError(&errorBuff)};
			throw std::runtime_error{
			        std::string{"Failed to retrieve required extensions due to error "} + errorBuff + '(' +
			        std::to_string(error) + ')'
			};
		}

		std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef ENABLE_VALIDATION_LAYERS
		extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

		extensions.insert(extensions.end(), INSTANCE_EXTENSIONS.cbegin(), INSTANCE_EXTENSIONS.cend());

		return extensions;
	}
#ifdef ENABLE_VALIDATION_LAYERS

	bool Instance::CheckValidationLayerSupport() {
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

	VkResult Instance::CreateDebugUtilsMessengerEXT(
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

	void Instance::DestroyDebugUtilsMessengerEXT(
	        VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator
	) {
		const auto func{reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
		)};

		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	void Instance::SetupDebugMessenger() {
		if (const VkResult result{CreateDebugUtilsMessengerEXT(
		            m_Instance, &DEBUG_UTILS_MESSENGER_CREATE_INFO, nullptr, &m_DebugMessenger
		    )};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to set up debug messenger: "} + string_VkResult(result)};
		}
	}
#endif
}// namespace roing::vk
