#ifndef PORTAL2RAYTRACED_APPLICATION_H
#define PORTAL2RAYTRACED_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include "Model.h"
#include "ModelInstance.h"
#include "src/utils/debug.h"
#include "vk/Device.h"
#include "vk/Instance.h"
#include "vk/QueueFamilyIndices.h"
#include "vk/Surface.h"
#include "vk/Window.h"
#include <GLFW/glfw3.h>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <string_view>
#include <vector>

// TODO: avoid vkAllocateMemory calls, instead group with custom allocator and use an offset

namespace roing {
	class Model;

	class Application final {
	public:
		Application()
		    : m_Window{WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE}
		    , m_Instance{}
		    , m_Surface{m_Instance, m_Window}
		    , m_PhysicalDevice{m_Instance.PickPhysicalDevice(m_Surface)}
		    , m_Device{
		              m_Surface,
		              m_PhysicalDevice,
		              m_Window,
		              {{"medieval_building.obj", glm::translate(glm::mat4{1.f}, glm::vec3{0, 0, -10.f})}}
		      } {};

		~Application() {
			DEBUG("Terminating application");
			glfwTerminate();
		}

		void Run();

	private:
		void InitVulkan();

		void MainLoop();

		void DrawFrame();

		static constexpr uint32_t         WINDOW_WIDTH{800}, WINDOW_HEIGHT{600};
		static constexpr std::string_view WINDOW_TITLE{"Vulkan"};
		const std::vector<Vertex>         VERTICES{
                Vertex{{-0.5f, -0.5f, 0.f}}, Vertex{{0.5f, -0.5f, 0.f}}, Vertex{{0.5f, 0.5f, 0.f}}
        };
		const std::vector<uint32_t> INDICES{0, 1, 2, 2, 3, 0};

		vk::Window       m_Window;
		vk::Instance     m_Instance;
		vk::Surface      m_Surface;
		VkPhysicalDevice m_PhysicalDevice;
		vk::Device       m_Device;

		std::vector<VkFramebuffer> m_SwapChainFramebuffers{};
	};
}// namespace roing


#endif//PORTAL2RAYTRACED_APPLICATION_H
