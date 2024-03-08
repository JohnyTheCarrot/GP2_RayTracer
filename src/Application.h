#ifndef PORTAL2RAYTRACED_APPLICATION_H
#define PORTAL2RAYTRACED_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include "Model.h"
#include "Vertex.h"
#include "vk/Device.h"
#include "vk/Instance.h"
#include "vk/QueueFamilyIndices.h"
#include "vk/Surface.h"
#include "vk/Window.h"
#include <GLFW/glfw3.h>
#include <array>
#include <glm\glm.hpp>
#include <optional>
#include <string_view>
#include <vector>

// TODO: avoid vkAllocateMemory calls, instead group with custom allocator and use an offset

namespace roing {
	class Model;

	class Application final {
	public:
		Application() {
		}

		void Run();

	private:
		// Main
		void InitWindow();

		void InitVulkan();

		void MainLoop();

		void DrawFrame();

		void Cleanup();

		void InitRayTracing();

		// Helper
		void PickPhysicalDevice();

		void ObjectToVkGeomtry(const Model &model);

		[[nodiscard]]
		VkShaderModule CreateShaderModule(std::vector<char> &&code);

		[[nodiscard]]
		bool IsDeviceSuitable(VkPhysicalDevice device);

		// Static
		[[nodiscard]]
		static bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

		static constexpr int                         WINDOW_WIDTH{800}, WINDOW_HEIGHT{600};
		static constexpr std::string_view            WINDOW_TITLE{"Vulkan"};
		static constexpr std::array<const char *, 0> INSTANCE_EXTENSIONS{};
		const std::vector<Vertex>                    VERTICES{
                Vertex{{-0.5f, -0.5f, 0.f}, {1.0f, 0.0f, 0.0f}}, Vertex{{0.5f, -0.5f, 0.f}, {0.0f, 1.0f, 0.0f}},
                Vertex{{0.5f, 0.5f, 0.f}, {0.0f, 0.0f, 1.0f}}, Vertex{{-0.5f, 0.5f, 0.f}, {1.0f, 1.0f, 1.0f}}
        };
		const std::vector<uint32_t> INDICES{0, 1, 2, 2, 3, 0};

		vk::Window                                      m_Window{};
		vk::Instance                                    m_Instance{};
		vk::Surface                                     m_Surface{};
		vk::Device                                      m_Device{};
		std::vector<Model>                              m_Models{};
		VkPhysicalDevice                                m_PhysicalDevice{};
		std::vector<VkFramebuffer>                      m_SwapChainFramebuffers{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RtProperties{
		        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		};
	};
}// namespace roing


#endif//PORTAL2RAYTRACED_APPLICATION_H
