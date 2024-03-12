#include "CommandBuffer.h"
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.hpp>

namespace roing::vk {
	CommandBuffer::CommandBuffer(VkDevice hDevice, CommandBufferHandle &&hCommandBuffer)
	    : m_CommandBufferHandle{std::move(hCommandBuffer)}
	    , m_hDevice{hDevice} {
	}

	void CommandBuffer::Begin(VkCommandBufferUsageFlags flags) const {
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags                    = flags;
		beginInfo.pInheritanceInfo         = nullptr;

		vkBeginCommandBuffer(m_CommandBufferHandle.get(), &beginInfo);
	}

	void CommandBuffer::End() {
		vkEndCommandBuffer(m_CommandBufferHandle.get());
	}

	void CommandBuffer::Reset() {
		vkResetCommandBuffer(m_CommandBufferHandle.get(), 0);
	}

	void CommandBuffer::Submit(VkSubmitInfo &submitInfo, VkQueue queue) {
		std::array<VkCommandBuffer, 1> commandBuffers{m_CommandBufferHandle.get()};

		submitInfo.commandBufferCount = commandBuffers.size();
		submitInfo.pCommandBuffers    = commandBuffers.data();

		if (const VkResult result{vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"vkQueueSubmit() failed: "} + string_VkResult(result)};
		}
	}

	void CommandBuffer::SubmitAndWait(VkSubmitInfo &submitInfo, VkQueue queue) {
		Submit(submitInfo, queue);

		if (const VkResult result{vkQueueWaitIdle(queue)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"vkQueueWaitIdle() failed: "} + string_VkResult(result)};
		}
	}

	void CommandBuffer::Submit(VkQueue queue) {
		VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};

		Submit(submitInfo, queue);
	}

	void CommandBuffer::SubmitAndWait(VkQueue queue) {
		VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};

		SubmitAndWait(submitInfo, queue);
	}

	VkCommandBuffer CommandBuffer::GetHandle() const noexcept {
		return m_CommandBufferHandle.get();
	}

	void CommandBuffer::BindPipeline(const PipelineHandle::pointer &hPipeline, VkPipelineBindPoint bindPoint) const {
		vkCmdBindPipeline(GetHandle(), bindPoint, hPipeline);
	}
}// namespace roing::vk
