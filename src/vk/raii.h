#ifndef PORTAL2RAYTRACED_RAII_H
#define PORTAL2RAYTRACED_RAII_H

#include "RayTracingExtFunctions.h"
#include <memory>
#include <vulkan/vulkan_core.h>

namespace roing::vk {
	class DeviceDestroyer final {
	public:
		void operator()(VkDevice hDevice) {
			vkDestroyDevice(hDevice, nullptr);
		}
	};

	using DeviceHandle = std::unique_ptr<VkDevice_T, DeviceDestroyer>;

	class ImageDestroyer final {
	public:
		explicit ImageDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkImage hImage) {
			vkDestroyImage(m_hDevice, hImage, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using ImageHandle = std::unique_ptr<VkImage_T, ImageDestroyer>;

	class CommandPoolDestroyer final {
	public:
		explicit CommandPoolDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkCommandPool hCommandPool) {
			vkDestroyCommandPool(m_hDevice, hCommandPool, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using CommandPoolHandle = std::unique_ptr<VkCommandPool_T, CommandPoolDestroyer>;

	class CommandBufferDestroyer final {
	public:
		CommandBufferDestroyer(VkDevice hDevice, const CommandPoolHandle &hCommandPool)
		    : m_hDevice{hDevice}
		    , m_hCommandPool{hCommandPool.get()} {
		}

		void operator()(VkCommandBuffer hCommandBuffer) {
			vkFreeCommandBuffers(m_hDevice, m_hCommandPool, 1, &hCommandBuffer);
		}

	private:
		VkDevice      m_hDevice;
		VkCommandPool m_hCommandPool;
	};

	using CommandBufferHandle = std::unique_ptr<VkCommandBuffer_T, CommandBufferDestroyer>;

	class AccelerationStructureDestroyer final {
	public:
		explicit AccelerationStructureDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkAccelerationStructureKHR hAccelerationStructure) {
			roing::vk::vkDestroyAccelerationStructureKHR(m_hDevice, hAccelerationStructure, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using AccelerationStructureHandle = std::unique_ptr<VkAccelerationStructureKHR_T, AccelerationStructureDestroyer>;

	class DescriptorSetLayoutDestroyer final {
	public:
		explicit DescriptorSetLayoutDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkDescriptorSetLayout hDescriptorSetLayout) {
			vkDestroyDescriptorSetLayout(m_hDevice, hDescriptorSetLayout, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using DescriptorSetLayoutHandle = std::unique_ptr<VkDescriptorSetLayout_T, DescriptorSetLayoutDestroyer>;

	class DescriptorSetDestroyer final {
	public:
		explicit DescriptorSetDestroyer(VkDevice hDevice, VkDescriptorPool hDescriptorPool)
		    : m_hDevice{hDevice}
		    , m_hDescriptorPool{hDescriptorPool} {
		}

		void operator()(VkDescriptorSet hDescriptorSet) {
			vkFreeDescriptorSets(m_hDevice, m_hDescriptorPool, 1, &hDescriptorSet);
		}

	private:
		VkDevice         m_hDevice;
		VkDescriptorPool m_hDescriptorPool;
	};

	using DescriptorSetHandle = std::unique_ptr<VkDescriptorSet_T, DescriptorSetDestroyer>;

	class DescriptorPoolDestroyer final {
	public:
		explicit DescriptorPoolDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkDescriptorPool hDescriptorPool) {
			vkDestroyDescriptorPool(m_hDevice, hDescriptorPool, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using DescriptorPoolHandle = std::unique_ptr<VkDescriptorPool_T, DescriptorPoolDestroyer>;

	class PipelineLayoutDestroyer final {
	public:
		explicit PipelineLayoutDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {};

		void operator()(VkPipelineLayout hPipelineLayout) {
			vkDestroyPipelineLayout(m_hDevice, hPipelineLayout, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using PipelineLayoutHandle = std::unique_ptr<VkPipelineLayout_T, PipelineLayoutDestroyer>;

	class PipelineDestroyer final {
	public:
		explicit PipelineDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkPipeline hPipeline) {
			vkDestroyPipeline(m_hDevice, hPipeline, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using PipelineHandle = std::unique_ptr<VkPipeline_T, PipelineDestroyer>;

	class ShaderModuleDestroyer final {
	public:
		explicit ShaderModuleDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {};

		void operator()(VkShaderModule hShaderModule) {
			vkDestroyShaderModule(m_hDevice, hShaderModule, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using ShaderModuleHandle = std::unique_ptr<VkShaderModule_T, ShaderModuleDestroyer>;

	class SemaphoreDestroyer final {
	public:
		explicit SemaphoreDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkSemaphore hSemaphore) {
			vkDestroySemaphore(m_hDevice, hSemaphore, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using SemaphoreHandle = std::unique_ptr<VkSemaphore_T, SemaphoreDestroyer>;

	class FenceDestroyer final {
	public:
		explicit FenceDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {};

		void operator()(VkFence hFence) {
			vkDestroyFence(m_hDevice, hFence, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using FenceHandle = std::unique_ptr<VkFence_T, FenceDestroyer>;

	class SwapchainDestroyer final {
	public:
		explicit SwapchainDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkSwapchainKHR hSwapchain) {
			vkDestroySwapchainKHR(m_hDevice, hSwapchain, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using SwapchainHandle = std::unique_ptr<VkSwapchainKHR_T, SwapchainDestroyer>;

	class SamplerDestroyer final {
	public:
		explicit SamplerDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkSampler hSampler) {
			vkDestroySampler(m_hDevice, hSampler, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using SamplerHandle = std::unique_ptr<VkSampler_T, SamplerDestroyer>;

	class ImageViewDestroyer final {
	public:
		explicit ImageViewDestroyer(VkDevice hDevice)
		    : m_hDevice{hDevice} {
		}

		void operator()(VkImageView hImageView) {
			vkDestroyImageView(m_hDevice, hImageView, nullptr);
		}

	private:
		VkDevice m_hDevice;
	};

	using ImageViewHandle = std::unique_ptr<VkImageView_T, ImageViewDestroyer>;

}// namespace roing::vk
#endif//PORTAL2RAYTRACED_RAII_H
