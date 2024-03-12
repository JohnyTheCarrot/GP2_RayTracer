#ifndef PORTAL2RAYTRACED_COMMANDPOOL_H
#define PORTAL2RAYTRACED_COMMANDPOOL_H

#include "CommandBuffer.h"
#include "raii.h"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace roing::vk {

	class Surface;

	class Device;

	class CommandPool final {
	public:
		CommandPool(const Device &device, const Surface &surface, VkPhysicalDevice physicalDevice);

		[[nodiscard]]
		CommandPoolHandle::pointer GetHandle() const noexcept;

		[[nodiscard]]
		std::vector<CommandBuffer>
		AllocateCommandBuffers(uint32_t num, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

		[[nodiscard]]
		CommandBuffer AllocateCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

	private:
		VkDevice          m_DeviceHandle;
		CommandPoolHandle m_CommandPoolHandle;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_COMMANDPOOL_H
