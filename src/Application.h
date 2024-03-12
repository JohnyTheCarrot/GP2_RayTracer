#ifndef PORTAL2RAYTRACED_APPLICATION_H
#define PORTAL2RAYTRACED_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include "Model.h"
#include "ModelInstance.h"
#include "src/utils/debug.h"
#include "src/vk/DescriptorPool.h"
#include "src/vk/DescriptorSetLayout.h"
#include "src/vk/RaytracingPipeline.h"
#include "vk/CommandPool.h"
#include "vk/DescriptorSet.h"
#include "vk/Device.h"
#include "vk/Instance.h"
#include "vk/QueueFamilyIndices.h"
#include "vk/RenderProcess.h"
#include "vk/Surface.h"
#include "vk/Window.h"
#include <GLFW/glfw3.h>
#include <array>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <string_view>
#include <vector>

// TODO: avoid vkAllocateMemory calls, instead group with custom allocator and use an offset

namespace roing {
	class Model;

	struct ModelLoadData final {
		std::string fileName;
		glm::mat4   transform;
	};

	struct BuildAccelerationStructure final {
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
		        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
		};
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
		};
		const VkAccelerationStructureBuildRangeInfoKHR *rangeInfo{};
		std::optional<vk::AccelerationStructure>        accelerationStructure;
	};

	class Application final {
	public:
		Application(
		        std::function<std::vector<Model>(const vk::Device &, const vk::CommandPool &, VkPhysicalDevice)>
		                                                                                     modelLoader,
		        const std::function<std::vector<ModelInstance>(const std::vector<Model> &)> &instanceLoader
		);

		~Application() {
			DEBUG("Terminating application");
			glfwTerminate();
		}

		void Run();

	private:
		void InitVulkan();

		void MainLoop();

		void DrawFrame();

		static std::vector<BuildAccelerationStructure> CreateBottomLevelAccelerationStructure(
		        const vk::Device &device, VkPhysicalDevice physicalDevice, vk::CommandPool &commandPool,
		        VkQueue graphicsQueue, const std::vector<Model> &models
		);

		static void CmdCreateBlas(
		        const vk::Device &device, VkPhysicalDevice hPhysicalDevice, vk::CommandBuffer &commandBuffer,
		        const std::vector<uint32_t> &indices, std::vector<BuildAccelerationStructure> &buildAs,
		        VkDeviceAddress scratchAddress, VkQueryPool queryPool
		);

		static std::vector<BuildAccelerationStructure> BuildBlas(
		        const vk::Device &device, VkPhysicalDevice hPhysicalDevice, vk::CommandPool &commandPool,
		        VkQueue graphicsQueue, const std::vector<BlasInput> &input
		);

		static vk::AccelerationStructure CreateTopLevelAccelerationStructure(
		        const vk::Device &device, VkPhysicalDevice physicalDevice, const vk::CommandPool &commandPool,
		        VkQueue graphicsQueue, const std::vector<BuildAccelerationStructure> &blas,
		        const std::vector<ModelInstance> &modelInstances
		);

		static vk::AccelerationStructure BuildTlas(
		        const vk::Device &device, const vk::CommandPool &commandPool, VkPhysicalDevice hPhysicalDevice,
		        VkQueue graphicsQueue, const std::vector<VkAccelerationStructureInstanceKHR> &instances,
		        VkBuildAccelerationStructureFlagsKHR flags
		);

		static vk::AccelerationStructure CmdCreateTlas(
		        const vk::Device &device, VkPhysicalDevice physicalDevice, vk::CommandBuffer &cmdBuf,
		        uint32_t countInstance, VkDeviceAddress instBufferAddr, vk::Buffer &scratchBuffer,
		        VkBuildAccelerationStructureFlagsKHR flags
		);

		[[nodiscard]]
		static VkDeviceAddress GetBlasDeviceAddress(
		        const vk::Device &device, const std::vector<BuildAccelerationStructure> &blas, uint32_t blasId
		);

		static VkQueue GetDeviceQueue(const vk::Device &device, uint32_t familyIndex);

		[[nodiscard]]
		static vk::DescriptorPool
		CreatePool(const vk::Device &device, uint32_t maxSets = 1, VkDescriptorPoolCreateFlags flags = 0);

		static void AddRequiredPoolSizes(std::vector<VkDescriptorPoolSize> &poolSizes, uint32_t numSets);

		[[nodiscard]]
		static std::vector<ObjDesc> CreateObjDescs(const std::vector<Model> &models);

		static constexpr uint32_t         WINDOW_WIDTH{1200}, WINDOW_HEIGHT{900};
		static constexpr std::string_view WINDOW_TITLE{"Vulkan"};
		const std::vector<Vertex>         VERTICES{
                Vertex{{-0.5f, -0.5f, 0.f}}, Vertex{{0.5f, -0.5f, 0.f}}, Vertex{{0.5f, 0.5f, 0.f}}
        };
		const std::vector<uint32_t> INDICES{0, 1, 2, 2, 3, 0};

		vk::Window                               m_Window;
		vk::Instance                             m_Instance;
		vk::Surface                              m_Surface;
		VkPhysicalDevice                         m_PhysicalDevice;
		vk::Device                               m_Device;
		vk::DescriptorPool                       m_DescPool;
		vk::DescriptorSetLayout                  m_DescSetLayout;
		vk::DescriptorSet                        m_DescSet;
		vk::CommandPool                          m_CommandPool;
		vk::QueueFamilyIndices                   m_QueueFamilyIndices{};
		VkQueue                                  m_GraphicsQueue{};
		VkQueue                                  m_PresentQueue{};
		std::vector<BuildAccelerationStructure>  m_Blas{};
		std::optional<vk::AccelerationStructure> m_Tlas{};
		vk::RenderProcess                        m_RenderProcess;
		vk::Buffer                               m_GlobalsBuffer;

		std::vector<Model>         m_Models{};
		std::vector<ModelInstance> m_ModelInstances{};

		std::vector<ObjDesc> m_ObjDescriptors{};

		vk::Buffer m_ObjDescBuffer;

		std::vector<VkFramebuffer> m_SwapChainFramebuffers{};
	};
}// namespace roing


#endif//PORTAL2RAYTRACED_APPLICATION_H
