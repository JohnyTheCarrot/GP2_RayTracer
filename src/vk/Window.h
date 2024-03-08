#ifndef PORTAL2RAYTRACED_WINDOW_H
#define PORTAL2RAYTRACED_WINDOW_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string_view>

namespace roing::vk {

	class Window final {
	public:
		Window() = default;

		Window(int width, int height, std::string_view title);

		~Window() noexcept;

		Window(const Window &) = delete;

		Window &operator=(const Window &) = delete;

		Window(Window &&other) noexcept
		    : m_pWindow(other.m_pWindow)
		    , m_WasResized{other.m_WasResized} {
			other.m_pWindow = nullptr;
		}

		Window &operator=(Window &&other) noexcept {
			m_pWindow       = other.m_pWindow;
			other.m_pWindow = nullptr;
			m_WasResized    = other.m_WasResized;
			return *this;
		}

		GLFWwindow &operator->() noexcept {
			return *m_pWindow;
		}

		GLFWwindow &operator*() noexcept {
			return *m_pWindow;
		}

		[[nodiscard]]
		GLFWwindow *Get() const noexcept {
			return m_pWindow;
		}

		bool PollWasResized();

	private:
		static void FramebufferResizeCallback(GLFWwindow *pWindow, int width, int height);

		bool        m_WasResized{false};
		GLFWwindow *m_pWindow{};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_WINDOW_H
