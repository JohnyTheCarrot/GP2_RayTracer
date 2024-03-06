#ifndef PORTAL2RAYTRACED_APPLICATION_H
#define PORTAL2RAYTRACED_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <glm\glm.hpp>
#include <optional>
#include <string_view>
#include <vector>

// TODO: avoid vkAllocateMemory calls, instead group with custom allocator and use an offset

struct Vertex {
	glm::vec2 pos{};
	glm::vec3 color{};

	constexpr static VkVertexInputBindingDescription GetBindingDescription();

	constexpr static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
};

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

	void CreateCommandBuffers();

	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void CreateSyncObjects();

	void RecreateSwapChain();

	void CleanupSwapChain();

	void CreateVertexBuffer();

	void CreateIndexBuffer();

	void CreateBuffer(
	        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer,
	        VkDeviceMemory &bufferMemory
	);

	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

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
	static void FramebufferResizeCallback(GLFWwindow *pWindow, int width, int height);

	[[nodiscard]]
	static std::vector<char> ReadFile(std::string_view fileName);

	[[nodiscard]]
	static bool CheckValidationLayerSupport();

	[[nodiscard]]
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

	[[nodiscard]]
	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

	static void PrintAvailableInstanceExtensions();

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
	static constexpr std::array<const char *, 0> INSTANCE_EXTENSIONS{};
	static constexpr std::array<const char *, 6> DEVICE_EXTENSIONS{
	        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
	        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	        VK_KHR_SPIRV_1_4_EXTENSION_NAME
	};
	static constexpr int                   MAX_FRAMES_IN_FLIGHT{2};
	static constexpr std::array<Vertex, 4> VERTICES{
	        Vertex{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, Vertex{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	        Vertex{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, Vertex{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};
	static constexpr std::array<uint16_t, 6> INDICES{0, 1, 2, 2, 3, 0};

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
	uint32_t                   m_CurrentFrame{0};
	bool                       m_FramebufferResized{false};
	VkBuffer                   m_VertexBuffer{};
	VkDeviceMemory             m_VertexBufferMemory{};
	VkBuffer                   m_IndexBuffer{};
	VkDeviceMemory             m_IndexBufferMemory{};

	std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_CommandBuffers{};
	std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     m_ImageAvailableSemaphores{};
	std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     m_RenderFinishedSemaphores{};
	std::array<VkFence, MAX_FRAMES_IN_FLIGHT>         m_InFlightFences{};
};


#endif//PORTAL2RAYTRACED_APPLICATION_H
