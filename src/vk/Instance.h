#ifndef PORTAL2RAYTRACED_INSTANCE_H
#define PORTAL2RAYTRACED_INSTANCE_H

#include "Surface.h"
#include <array>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

#ifndef NDEBUG
#define ENABLE_VALIDATION_LAYERS
#endif

namespace roing::vk {

	class Instance final {
	public:
		Instance();

		~Instance() noexcept;

		Instance(const Instance &)            = delete;
		Instance &operator=(const Instance &) = delete;

		Instance(Instance &&other) noexcept
		    : m_Instance{other.m_Instance}
#ifdef ENABLE_VALIDATION_LAYERS
		    , m_DebugMessenger{other.m_DebugMessenger}
#endif
		{
			other.m_Instance = nullptr;
		}

		Instance &operator=(Instance &&other) noexcept {
			m_Instance = other.m_Instance;
#ifdef ENABLE_VALIDATION_LAYERS
			m_DebugMessenger = other.m_DebugMessenger;
#endif

			other.m_Instance = nullptr;

			return *this;
		}

		[[nodiscard]]
		VkInstance Get() const noexcept {
			return m_Instance;
		}

		[[nodiscard]]
		VkPhysicalDevice PickPhysicalDevice(const Surface &surface) const;

#ifdef ENABLE_VALIDATION_LAYERS
		static constexpr bool VALIDATION_LAYERS_ENABLED{true};
#else
		static constexpr bool VALIDATION_LAYERS_ENABLED{false};
#endif

	private:
		[[nodiscard]]
		static bool IsDeviceSuitable(VkPhysicalDevice device, const Surface &surface) noexcept;

		[[nodiscard]]
		static bool CheckDeviceExtensionSupport(VkPhysicalDevice device) noexcept;

		static VKAPI_ATTR VkBool32 DebugCallback(
		        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData
		);

		static void PrintAvailableInstanceExtensions();

		[[nodiscard]]
		static std::vector<const char *> GetRequiredExtensions();


#ifdef ENABLE_VALIDATION_LAYERS
		[[nodiscard]]
		static bool CheckValidationLayerSupport();

		[[nodiscard]]
		static VkResult CreateDebugUtilsMessengerEXT(
		        VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
		        const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger
		);

		static void DestroyDebugUtilsMessengerEXT(
		        VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator
		);

		void SetupDebugMessenger();
#endif

		static constexpr std::array<const char *, 1> VALIDATION_LAYERS{"VK_LAYER_KHRONOS_validation"};
		static constexpr std::array<const char *, 0> INSTANCE_EXTENSIONS{};

		static constexpr VkDebugUtilsMessengerCreateInfoEXT DEBUG_UTILS_MESSENGER_CREATE_INFO{
		        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		        .pfnUserCallback = DebugCallback,
		        .pUserData       = nullptr
		};

		VkInstance m_Instance{};
#ifdef ENABLE_VALIDATION_LAYERS
		VkDebugUtilsMessengerEXT m_DebugMessenger{};
#endif
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_INSTANCE_H
