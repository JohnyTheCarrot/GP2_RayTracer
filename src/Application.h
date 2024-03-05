#ifndef PORTAL2RAYTRACED_APPLICATION_H
#define PORTAL2RAYTRACED_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily{};

	[[nodiscard]]
	bool IsComplete() const noexcept {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class Application final {
public:
	void Run();

	static VKAPI_ATTR VkBool32 DebugCallback(
	        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData
	);

private:
	// Main
	void InitWindow();

	void InitVulkan();

	void MainLoop();

	void Cleanup();

	// Helper
	void CreateInstance();

	void SetupDebugMessenger();

	void PickPhysicalDevice();

	void CreateLogicalDevice();

	void CreateSurface();

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	[[nodiscard]]
	bool IsDeviceSuitable(VkPhysicalDevice device);

	// Static
	[[nodiscard]]
	static bool CheckValidationLayerSupport();

	static void PrintAvailableExtensions();

	[[nodiscard]]
	static std::vector<const char *> GetRequiredExtensions();

	[[nodiscard]]
	static VkResult CreateDebugUtilsMessengerEXT(
	        VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
	        const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger
	);

	static void DestroyDebugUtilsMessengerEXT(
	        VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator
	);

	static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

#ifdef NDEBUG
	static constexpr bool ENABLE_VALIDATION_LAYERS{false};
#else
	static constexpr bool ENABLE_VALIDATION_LAYERS{true};
#endif

	static constexpr int                         WINDOW_WIDTH{800}, WINDOW_HEIGHT{600};
	static constexpr std::string_view            WINDOW_TITLE{"Vulkan"};
	static constexpr std::array<const char *, 1> VALIDATION_LAYERS{"VK_LAYER_KHRONOS_validation"};

	VkInstance               m_Instance{};
	VkDebugUtilsMessengerEXT m_DebugMessenger{};
	GLFWwindow              *m_pWindow{};
	VkPhysicalDevice         m_PhysicalDevice{VK_NULL_HANDLE};
	VkDevice                 m_Device{};
	VkSurfaceKHR             m_Surface{};
	VkQueue                  m_GraphicsQueue{};
	VkQueue                  m_PresentQueue{};
};


#endif//PORTAL2RAYTRACED_APPLICATION_H
