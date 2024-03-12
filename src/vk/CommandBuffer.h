#ifndef PORTAL2RAYTRACED_COMMANDBUFFER_H
#define PORTAL2RAYTRACED_COMMANDBUFFER_H

#include "raii.h"

namespace roing::vk {

	class CommandBuffer final {
	public:
		CommandBuffer(VkDevice hDevice, CommandBufferHandle &&hCommandBuffer);

		void Begin(VkCommandBufferUsageFlags flags) const;

		void End();

		void Reset();

		void Submit(VkSubmitInfo &submitInfo, VkQueue queue);

		void SubmitAndWait(VkSubmitInfo &submitInfo, VkQueue queue);

		void Submit(VkQueue queue);

		void SubmitAndWait(VkQueue queue);

		void BindPipeline(const PipelineHandle::pointer &hPipeline, VkPipelineBindPoint bindPoint) const;

		[[nodiscard]]
		VkCommandBuffer GetHandle() const noexcept;

	private:
		VkDevice            m_hDevice;
		CommandBufferHandle m_CommandBufferHandle;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_COMMANDBUFFER_H
