#include "Window.h"

namespace roing::vk {
	Window::Window(int width, int height, std::string_view title) {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_pWindow = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
		glfwSetWindowUserPointer(m_pWindow, this);
		glfwSetFramebufferSizeCallback(m_pWindow, FramebufferResizeCallback);
	}

	bool Window::PollWasResized() {
		if (!m_WasResized)
			return false;

		m_WasResized = false;
		return true;
	}

	void Window::FramebufferResizeCallback(GLFWwindow *pApplication, int, int) {
		auto appPtr{reinterpret_cast<Window *>(pApplication)};
		appPtr->m_WasResized = true;
	}

	Window::~Window() noexcept {
		if (!m_pWindow)
			return;

		glfwDestroyWindow(m_pWindow);
		m_pWindow = nullptr;
	}
}// namespace roing::vk
