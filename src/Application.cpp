#include "Application.h"
#include "src/vk/PhysicalDeviceUtils.h"
#include "tiny_obj_loader.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing {
	void Application::Run() {
		InitVulkan();
		MainLoop();
	}

	void Application::InitVulkan() {
		//		InitRayTracing();
		//		m_Device.SetUpRaytracing(m_PhysicalDevice, m_Models, m_ModelInstances);
	}

	void Application::MainLoop() {
		while (!glfwWindowShouldClose(m_Window.Get())) {
			glfwPollEvents();

			DrawFrame();
		}

		vkDeviceWaitIdle(m_Device.GetHandle());
	}

	void Application::DrawFrame() {
		if (m_Device.DrawFrame(m_Window)) {
			m_Device.UpdatePostDescriptorSet();
			m_Device.UpdateRtDescriptorSet();
			m_Device.RecreateSwapchain(m_Window, m_Surface, m_PhysicalDevice);
		}
	}
}// namespace roing
