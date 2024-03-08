#include "Application.h"
#include "src/vk/PhysicalDeviceUtils.h"
#include "tiny_obj_loader.h"
#include "utils/ToTransformMatrixKHR.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing {
	void Application::Run() {
		InitWindow();
		InitVulkan();
		MainLoop();
	}

	void Application::InitWindow() {
		m_Window = {WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE};
	}

	void Application::InitVulkan() {
		m_Instance.Init();

		m_Surface = vk::Surface{m_Instance, m_Window};

		m_PhysicalDevice = m_Instance.PickPhysicalDevice(m_Surface);

		m_Device = vk::Device{m_Surface, m_PhysicalDevice};

		m_Device.CreateSwapchain(m_Window, m_Surface, m_PhysicalDevice, &m_Swapchain);

		m_Swapchain.CreateImageViews();
		m_Swapchain.CreateRenderPass();
		m_Swapchain.CreateGraphicsPipeline();
		m_Swapchain.CreateFramebuffers();
		m_Device.CreateCommandPool(m_Surface, m_PhysicalDevice);
		const auto &model{m_Models.emplace_back(m_PhysicalDevice, m_Device, VERTICES, INDICES)};
		m_ModelInstances.emplace_back(&model, glm::mat4(1.0f), 0);

		m_Swapchain.CreateCommandBuffers();
		m_Swapchain.CreateSyncObjects();

		InitRayTracing();
		m_Device.SetUpRaytracing(m_PhysicalDevice, m_Models, m_ModelInstances);
	}

	void Application::MainLoop() {
		while (!glfwWindowShouldClose(m_Window.Get())) {
			glfwPollEvents();
			DrawFrame();
		}

		vkDeviceWaitIdle(m_Device.GetHandle());
	}

	void Application::DrawFrame() {
		if (m_Swapchain.DrawFrame(m_Window, m_Models))
			m_Device.RecreateSwapchain(m_Window, m_Surface, m_PhysicalDevice, &m_Swapchain);
	}

	void Application::InitRayTracing() {
		VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		properties2.pNext = &m_RtProperties;
		vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &properties2);
	}

}// namespace roing
