#ifndef PORTAL2RAYTRACED_DEVICE_H
#define PORTAL2RAYTRACED_DEVICE_H

#include "../Model.h"
#include "../ModelInstance.h"
#include "AccelerationStructure.h"
#include "Buffer.h"
#include "QueueFamilyIndices.h"
#include "Surface.h"
#include "Swapchain.h"
#include <array>
#include <optional>
#include <vulkan/vulkan.h>

namespace roing::vk {
	enum class SceneBindings { eGlobals = 0, eObjDescs = 1, eTextures = 2, eImplicits = 3 };
	enum class RtxBindings { eTlas = 0, eOutImage = 1 };
	enum class DescriptorSupport : uint32_t { CORE_1_0 = 0, CORE_1_2 = 1, INDEXING_EXT = 2 };

	struct ModelLoadData final {
		std::string fileName;
		glm::mat4   transform;
	};

	void vkDestroyAccelerationStructureKHR(
	        VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks *pAllocator
	);

	void vkCmdCopyAccelerationStructureKHR(
	        VkDevice device, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR *copyInfo
	);

	VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(
	        VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR *addressInfo
	);

	struct BuildAccelerationStructure final {
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
		};
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
		};
		const VkAccelerationStructureBuildRangeInfoKHR *rangeInfo{};
		AccelerationStructure                           accelerationStructure;
	};

	class Device final {
	public:
		Device(const Surface &surface, VkPhysicalDevice physicalDevice, const Window &window,
		       const std::vector<ModelLoadData> &models);

		~Device() noexcept;

		Device(const Device &)            = delete;
		Device &operator=(const Device &) = delete;

		Device(Device &&)            = delete;
		Device &operator=(Device &&) = delete;

		void CreateRtDescriptorSet();

		void UpdateRtDescriptorSet();

		void UpdatePostDescriptorSet();

		[[nodiscard]]
		VkDevice GetHandle() const noexcept {
			return m_Device;
		}

		void RecreateSwapchain(const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice);

		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

		void QueueSubmit(uint32_t submitCount, const VkSubmitInfo *pSubmitInfo, VkFence fence);

		// Return value: whether to recreate the swap chain
		bool QueuePresent(const VkPresentInfoKHR *pPresentInfo, Window &window);

		static constexpr std::array<const char *, 6> DEVICE_EXTENSIONS{
		        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		        VK_KHR_SPIRV_1_4_EXTENSION_NAME
		};

		void CreateCommandPool(const Surface &surface, VkPhysicalDevice physicalDevice);

		[[nodiscard]]
		Buffer CreateBuffer(
		        VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
		        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		) const;

		[[nodiscard]]
		VkCommandPool GetCommandPoolHandle() const noexcept {
			return m_CommandPool;
		}

		void BuildBlas(
		        VkPhysicalDevice physicalDevice, const std::vector<BlasInput> &input,
		        VkBuildAccelerationStructureFlagsKHR flags, std::vector<BuildAccelerationStructure> &blasOut
		);

		void BuildTlas(
		        VkPhysicalDevice physicalDevice, const std::vector<VkAccelerationStructureInstanceKHR> &instances,
		        VkBuildAccelerationStructureFlagsKHR flags  = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		        bool                                 update = false
		);

		void SetUpRaytracing(
		        VkPhysicalDevice physicalDevice, const std::vector<Model> &models,
		        const std::vector<ModelInstance> &modelInstances
		);

		[[nodiscard]]
		VkCommandBuffer CreateCommandBuffer();

		void SubmitAndWait(VkCommandBuffer commandBuffer);

		void Submit(VkCommandBuffer commandBuffer);

		[[nodiscard]]
		vk::AccelerationStructure &GetTlas() noexcept {
			return *m_Tlas;
		}

		[[nodiscard]]
		Swapchain &GetSwapchain() noexcept {
			return m_Swapchain;
		}

		[[nodiscard]]
		VkDescriptorSet GetDescriptorSet() const noexcept {
			return m_DescSet;
		}

		[[nodiscard]]
		QueueFamilyIndices GetQueueFamilyIndices() const noexcept {
			return m_QueueFamilyIndices;
		}

		[[nodiscard]]
		bool DrawFrame(Window &window);

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


	private:
		void FillRtDescriptorSet();

		void CreatePostDescriptorSet();

		void CreatePostPipeline();

		void CreatePostRenderPass();

		[[nodiscard]]
		Model LoadModel(VkPhysicalDevice physicalDevice, const std::string &fileName);

		[[nodiscard]]
		VkDescriptorPool CreatePool(uint32_t maxSets = 1, VkDescriptorPoolCreateFlags flags = 0);

		[[nodiscard]]
		VkDescriptorSetLayout CreateDescriptorSetLayout(
		        VkDescriptorSetLayoutCreateFlags flags, std::vector<VkDescriptorBindingFlags> &bindingFlags,
		        std::vector<VkDescriptorSetLayoutBinding> &bindings
		);

		void AddRequiredPoolSizes(std::vector<VkDescriptorPoolSize> &poolSizes, uint32_t numSets);

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
		VkDeviceAddress GetBlasDeviceAddress(uint32_t blasId) const;

		void CreateBottomLevelAccelerationStructure(VkPhysicalDevice physicalDevice, const std::vector<Model> &models);

		void CreateTopLevelAccelerationStructure(
		        VkPhysicalDevice physicalDevice, const std::vector<ModelInstance> &modelInstances
		);

		void CmdCreateBlas(
		        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, const std::vector<uint32_t> &indices,
		        std::vector<BuildAccelerationStructure> &buildAs, VkDeviceAddress scratchAddress, VkQueryPool queryPool
		);

		void CmdCompactBlas(
		        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
		        std::vector<BuildAccelerationStructure> &buildAs, VkQueryPool queryPool
		);

		void CmdCreateTlas(
		        VkPhysicalDevice physicalDevice, VkCommandBuffer cmdBuf, uint32_t countInstance,
		        VkDeviceAddress instBufferAddr, Buffer &scratchBuffer, VkBuildAccelerationStructureFlagsKHR flags,
		        bool update, bool motion
		);

		[[nodiscard]]
		AccelerationStructure
		CreateAccelerationStructure(VkPhysicalDevice physicalDevice, VkAccelerationStructureCreateInfoKHR &createInfo);

		static void vkGetAccelerationStructureBuildSizesKHR(
		        VkDevice device, VkAccelerationStructureBuildTypeKHR buildType,
		        const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo, const uint32_t *pMaxPrimitiveCounts,
		        VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo
		);

		static VkResult vkCreateAccelerationStructureKHR(
		        VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
		        const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure
		);

		static void vkCmdBuildAccelerationStructuresKHR(
		        VkDevice device, VkCommandBuffer commandBuffer, uint32_t infoCount,
		        const VkAccelerationStructureBuildGeometryInfoKHR     *pInfos,
		        const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos
		);

		static void vkCmdWriteAccelerationStructuresPropertiesKHR(
		        VkDevice device, VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount,
		        const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool,
		        uint32_t firstQuery
		);

		static constexpr std::array<const char *, 1> VALIDATION_LAYERS{"VK_LAYER_KHRONOS_validation"};

		VkDevice            m_Device{};
		VkCommandPool       m_CommandPool{};
		VkQueue             m_GraphicsQueue{};
		VkQueue             m_PresentQueue{};
		std::vector<Buffer> m_AccelerationStructureBuffers{};
		Swapchain           m_Swapchain;

		std::vector<VkDescriptorSetLayoutBinding> m_DescriptorSetLayoutBindings{};
		std::vector<VkDescriptorBindingFlags>     m_DescriptorBindingFlags{};
		VkDescriptorPool                          m_DescPool{};
		VkDescriptorSetLayout                     m_DescSetLayout{};
		VkDescriptorSet                           m_DescSet{};

		QueueFamilyIndices m_QueueFamilyIndices{};

		std::vector<Model>                      m_Models;
		std::vector<ModelInstance>              m_ModelInstances;
		std::vector<BuildAccelerationStructure> m_Blas{};
		std::optional<AccelerationStructure>    m_Tlas{};
	};
}// namespace roing::vk

#endif//PORTAL2RAYTRACED_DEVICE_H
