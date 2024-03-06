#ifndef PORTAL2RAYTRACED_APPLICATION_H
#define PORTAL2RAYTRACED_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily{};
	std::optional<uint32_t> presentFamily{};

	[[nodiscard]]
	bool IsComplete() const noexcept {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR        capabilities{};
	std::vector<VkSurfaceFormatKHR> formats{};
	std::vector<VkPresentModeKHR>   presentModes{};
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

	void DrawFrame();

	void Cleanup();

	// Helper
	void CreateInstance();

	void SetupDebugMessenger();

	void PickPhysicalDevice();

	void CreateLogicalDevice();

	void CreateSurface();

	void CreateSwapChain();

	void CreateImageViews();

	void CreateGraphicsPipeline();

	void CreateRenderPass();

	void CreateFramebuffers();

	void CreateCommandPool();

	void CreateCommandBuffer();

	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void CreateSyncObjects();

	[[nodiscard]]
	VkShaderModule CreateShaderModule(std::vector<char> &&code);

	[[nodiscard]]
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	[[nodiscard]]
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

	[[nodiscard]]
	bool IsDeviceSuitable(VkPhysicalDevice device);

	[[nodiscard]]
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	// Static
	[[nodiscard]]
	static std::vector<char> ReadFile(std::string_view fileName);

	[[nodiscard]]
	static bool CheckValidationLayerSupport();

	[[nodiscard]]
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

	[[nodiscard]]
	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

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

	[[nodiscard]]
	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

#ifdef NDEBUG
	static constexpr bool ENABLE_VALIDATION_LAYERS{false};
#else
	static constexpr bool ENABLE_VALIDATION_LAYERS{true};
#endif

	static constexpr int                         WINDOW_WIDTH{800}, WINDOW_HEIGHT{600};
	static constexpr std::string_view            WINDOW_TITLE{"Vulkan"};
	static constexpr std::array<const char *, 1> VALIDATION_LAYERS{"VK_LAYER_KHRONOS_validation"};
	static constexpr std::array<const char *, 1> DEVICE_EXTENSIONS{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkInstance                 m_Instance{};
	VkDebugUtilsMessengerEXT   m_DebugMessenger{};
	GLFWwindow                *m_pWindow{};
	VkPhysicalDevice           m_PhysicalDevice{VK_NULL_HANDLE};
	VkDevice                   m_Device{};
	VkSurfaceKHR               m_Surface{};
	VkQueue                    m_GraphicsQueue{};
	VkQueue                    m_PresentQueue{};
	VkSwapchainKHR             m_SwapChain{};
	std::vector<VkImage>       m_SwapChainImages{};
	VkFormat                   m_SwapChainImageFormat{};
	VkExtent2D                 m_SwapChainExtent{};
	std::vector<VkImageView>   m_SwapChainImageViews{};
	VkRenderPass               m_RenderPass{};
	VkPipelineLayout           m_PipelineLayout{};
	VkPipeline                 m_GraphicsPipeline{};
	std::vector<VkFramebuffer> m_SwapChainFramebuffers{};
	VkCommandPool              m_CommandPool{};
	VkCommandBuffer            m_CommandBuffer{};
	VkSemaphore                m_ImageAvailableSemaphore{};
	VkSemaphore                m_RenderFinishedSemaphore{};
	VkFence                    m_InFlightFence{};
};


#endif//PORTAL2RAYTRACED_APPLICATION_H
