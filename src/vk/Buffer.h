#ifndef PORTAL2RAYTRACED_BUFFER_H
#define PORTAL2RAYTRACED_BUFFER_H

#include <vulkan/vulkan_core.h>

namespace roing::vk {

	class Device;

	class Buffer final {
	public:
		Buffer() = default;

		Buffer(const Device &device, VkBuffer buffer, VkDeviceMemory memory);

		~Buffer();

		Buffer(const Buffer &)            = delete;
		Buffer &operator=(const Buffer &) = delete;

		Buffer(Buffer &&other) noexcept
		    : m_Device{other.m_Device}
		    , m_Buffer{other.m_Buffer}
		    , m_BufferMemory{other.m_BufferMemory} {
			other.m_Buffer = VK_NULL_HANDLE;
		};

		Buffer &operator=(Buffer &&other) noexcept {
			m_Device       = other.m_Device;
			m_Buffer       = other.m_Buffer;
			m_BufferMemory = other.m_BufferMemory;
			other.m_Buffer = VK_NULL_HANDLE;

			return *this;
		}

		[[nodiscard]]
		VkBuffer GetHandle() const noexcept {
			return m_Buffer;
		}

		[[nodiscard]]
		VkDeviceMemory GetMemory() const noexcept {
			return m_BufferMemory;
		}

		[[nodiscard]]
		VkDeviceAddress GetDeviceAddress() const noexcept;

	private:
		VkDevice       m_Device{};
		VkBuffer       m_Buffer{};
		VkDeviceMemory m_BufferMemory{};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_BUFFER_H
