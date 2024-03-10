#ifndef PORTAL2RAYTRACED_WINDOW_H
#define PORTAL2RAYTRACED_WINDOW_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string_view>

namespace roing::vk {

	class Window final {
	public:
		Window(uint32_t width, uint32_t height, std::string_view title);

		~Window() noexcept;

		Window(const Window &)            = delete;
		Window &operator=(const Window &) = delete;

		Window(Window &&)            = delete;
		Window &operator=(Window &&) = delete;

		[[nodiscard]]
		GLFWwindow *Get() const noexcept {
			return m_pWindow;
		}

		[[nodiscard]]
		bool PollWasResized();

		[[nodiscard]]
		bool WasResized() const noexcept {
			return m_WasResized;
		}

		[[nodiscard]]
		uint32_t GetWidth() const noexcept {
			return m_Width;
		}

		[[nodiscard]]
		uint32_t GetHeight() const noexcept {
			return m_Height;
		}

		GLFWwindow *GetWindowPtr() const noexcept {
			return m_pWindow;
		}

	private:
		static void FramebufferResizeCallback(GLFWwindow *pWindow, int width, int height);

		bool        m_WasResized{false};
		GLFWwindow *m_pWindow{};
		uint32_t    m_Width{0};
		uint32_t    m_Height{0};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_WINDOW_H
