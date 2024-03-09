#include "Window.h"
#include "src/utils/debug.h"

namespace roing::vk {
	Window::Window(uint32_t width, uint32_t height, std::string_view title)
	    : m_Width{width}
	    , m_Height{height} {
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

	void Window::FramebufferResizeCallback(GLFWwindow *pApplication, int width, int height) {
		auto appPtr{reinterpret_cast<Window *>(pApplication)};
		appPtr->m_WasResized = true;
	}

	Window::~Window() noexcept {
		if (!m_pWindow)
			return;

		DEBUG("Destroying window...");

		glfwDestroyWindow(m_pWindow);
		m_pWindow = nullptr;

		DEBUG("Window destroyed");
	}
}// namespace roing::vk
