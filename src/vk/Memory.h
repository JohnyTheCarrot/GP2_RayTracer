#ifndef PORTAL2RAYTRACED_MEMORY_H
#define PORTAL2RAYTRACED_MEMORY_H

#include <vulkan/vulkan.hpp>

namespace roing::vk {

	class Device;

	class Memory final {
	public:
		Memory() = default;

		Memory(const Device &device, VkDeviceMemory memory);

		~Memory();

		void Destroy();

		Memory(const Memory &)            = delete;
		Memory &operator=(const Memory &) = delete;

		Memory(Memory &&other) noexcept;
		Memory &operator=(Memory &&other) noexcept;

		[[nodiscard]]
		VkDeviceMemory GetHandle() const noexcept;

	private:
		VkDevice       m_Device{};
		VkDeviceMemory m_Memory{};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_MEMORY_H
