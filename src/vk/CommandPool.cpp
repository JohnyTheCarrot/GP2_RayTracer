#include "CommandPool.h"
#include "Device.h"
#include "PhysicalDeviceUtils.h"
#include "QueueFamilyIndices.h"
#include "raii.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	CommandPool::CommandPool(const Device &device, const Surface &surface, VkPhysicalDevice physicalDevice)
	    : m_CommandPoolHandle{VK_NULL_HANDLE, CommandPoolDestroyer(device.GetHandle())} {
		QueueFamilyIndices queueFamilyIndices{vk::physical_device::FindQueueFamilies(surface, physicalDevice)};

		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		VkCommandPool commandPool;
		if (const VkResult result{vkCreateCommandPool(device.GetHandle(), &commandPoolCreateInfo, nullptr, &commandPool)
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create command pool: "} + string_VkResult(result)};
		}
		m_CommandPoolHandle.reset(commandPool);
	}

	CommandPoolHandle::pointer CommandPool::GetHandle() const noexcept {
		return m_CommandPoolHandle.get();
	}

	std::vector<CommandBuffer> CommandPool::AllocateCommandBuffers(uint32_t num, VkCommandBufferLevel level) const {
		VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		allocInfo.level                       = level;
		allocInfo.commandPool                 = GetHandle();
		allocInfo.commandBufferCount          = num;

		std::vector<VkCommandBuffer> cmd(num);
		vkAllocateCommandBuffers(m_DeviceHandle, &allocInfo, cmd.data());

		std::vector<CommandBuffer> cmdBuffers;
		cmdBuffers.reserve(num);

		std::transform(cmd.begin(), cmd.end(), std::back_inserter(cmdBuffers), [&](VkCommandBuffer cmd) {
			return CommandBuffer{
			        m_DeviceHandle,
			        CommandBufferHandle{cmd, CommandBufferDestroyer{m_DeviceHandle, m_CommandPoolHandle}},
			};
		});

		return cmdBuffers;
	}

	CommandBuffer CommandPool::AllocateCommandBuffer(VkCommandBufferLevel level) const {
		auto bufferVec{AllocateCommandBuffers(1, level)};

		return std::move(bufferVec.front());
	}
}// namespace roing::vk
