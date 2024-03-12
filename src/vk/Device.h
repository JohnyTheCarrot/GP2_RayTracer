#ifndef PORTAL2RAYTRACED_DEVICE_H
#define PORTAL2RAYTRACED_DEVICE_H

#include "../Model.h"
#include "../ModelInstance.h"
#include "AccelerationStructure.h"
#include "Buffer.h"
#include "CommandPool.h"
#include "HostDevice.h"
#include "QueueFamilyIndices.h"
#include "RenderProcess.h"
#include "Surface.h"
#include "raii.h"
#include <array>
#include <optional>
#include <vulkan/vulkan.h>

namespace roing::vk {
	void vkCmdCopyAccelerationStructureKHR(
	        VkDevice device, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR *copyInfo
	);

	VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(
	        VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR *addressInfo
	);

	class Device final {
	public:
		Device(const Surface &surface, VkPhysicalDevice physicalDevice, const Window &window);

		~Device() noexcept;

		Device(const Device &)            = delete;
		Device &operator=(const Device &) = delete;

		Device(Device &&)            = delete;
		Device &operator=(Device &&) = delete;

		void UpdateRtDescriptorSet();

		void UpdatePostDescriptorSet();

		[[nodiscard]]
		VkDevice GetHandle() const noexcept {
			return m_Device.get();
		}

		void RecreateSwapchain(const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void
		CopyBuffer(const CommandPool &commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;

		void QueueSubmit(uint32_t submitCount, const VkSubmitInfo *pSubmitInfo, VkFence fence);

		// Return value: whether to recreate the swap chain
		bool QueuePresent(const VkPresentInfoKHR *pPresentInfo, Window &window);

		static constexpr std::array<const char *, 8> DEVICE_EXTENSIONS{
		        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
		        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
		        VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
		};

		[[nodiscard]]
		Buffer CreateBuffer(
		        VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
		        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		) const;

		[[nodiscard]]
		QueueFamilyIndices GetQueueFamilyIndices() const noexcept {
			return m_QueueFamilyIndices;
		}

		static uint32_t
		FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

		std::vector<VkDescriptorSetLayoutBinding> m_PostDescriptorSetLayoutBindings{};
		std::vector<VkDescriptorBindingFlags>     m_PostDescriptorSetLayoutBindingFlags{};
		VkDescriptorPool                          m_PostDescriptorPool{};
		VkDescriptorSetLayout                     m_PostDescriptorSetLayout{};
		VkDescriptorSet                           m_PostDescriptorSet{};
		VkPipeline                                m_PostPipeline{};
		VkPipelineLayout                          m_PostPipelineLayout{};
		VkRenderPass                              m_RenderPass{};

		std::vector<Model>         m_Models;
		std::vector<ModelInstance> m_ModelInstances;

	private:
		void FillRtDescriptorSet();

		void CreatePostDescriptorSet();

		void CreatePostPipeline();

		void CreatePostRenderPass();

		[[nodiscard]]
		static VkWriteDescriptorSet MakeWrite(
		        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
		        uint32_t arrayElement
		);

		[[nodiscard]]
		static VkWriteDescriptorSet MakeWrite(
		        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
		        const VkWriteDescriptorSetAccelerationStructureKHR *pAccelerationStructure, uint32_t arrayElement
		);

		[[nodiscard]]
		static VkWriteDescriptorSet MakeWrite(
		        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
		        const VkDescriptorImageInfo *pImageInfo, uint32_t arrayElement
		);

		[[nodiscard]]
		static VkWriteDescriptorSet MakeWrite(
		        const std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSet dstSet, uint32_t dstBinding,
		        const VkDescriptorBufferInfo *pBufferInfo, uint32_t arrayElement
		);

		[[nodiscard]]
		VkDeviceAddress GetBlasDeviceAddress(uint32_t blasId) const;

		void CreateTopLevelAccelerationStructure(
		        VkPhysicalDevice physicalDevice, const std::vector<ModelInstance> &modelInstances
		);

		void CmdCompactBlas(
		        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
		        std::vector<BuildAccelerationStructure> &buildAs, VkQueryPool queryPool
		);

		static void vkGetAccelerationStructureBuildSizesKHR(
		        VkDevice device, VkAccelerationStructureBuildTypeKHR buildType,
		        const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo, const uint32_t *pMaxPrimitiveCounts,
		        VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo
		);

		static constexpr std::array<const char *, 1> VALIDATION_LAYERS{"VK_LAYER_KHRONOS_validation"};

		DeviceHandle        m_Device{};
		VkQueue             m_GraphicsQueue{};
		VkQueue             m_PresentQueue{};
		std::vector<Buffer> m_AccelerationStructureBuffers{};

		std::vector<VkDescriptorBindingFlags> m_DescriptorBindingFlags{};

		QueueFamilyIndices m_QueueFamilyIndices{};
	};
}// namespace roing::vk

#endif//PORTAL2RAYTRACED_DEVICE_H
