#ifndef PORTAL2RAYTRACED_APPLICATION_H
#define PORTAL2RAYTRACED_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include "Model.h"
#include "ModelInstance.h"
#include "Vertex.h"
#include "src/utils/debug.h"
#include "vk/Device.h"
#include "vk/Instance.h"
#include "vk/QueueFamilyIndices.h"
#include "vk/Surface.h"
#include "vk/Window.h"
#include <GLFW/glfw3.h>
#include <array>
#include <glm\glm.hpp>
#include <optional>
#include <string_view>
#include <vector>

// TODO: avoid vkAllocateMemory calls, instead group with custom allocator and use an offset

namespace roing {
	class Model;

	enum class SceneBindings { eGlobals = 0, eObjDescs = 1, eTextures = 2, eImplicits = 3 };
	enum class RtxBindings { eTlas = 0, eOutImage = 1 };
	enum class DescriptorSupport : uint32_t { CORE_1_0 = 0, CORE_1_2 = 1, INDEXING_EXT = 2 };

	class Application final {
	public:
		Application()
		    : m_Window{WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE}
		    , m_Instance{}
		    , m_Surface{m_Instance, m_Window}
		    , m_PhysicalDevice{m_Instance.PickPhysicalDevice(m_Surface)}
		    , m_Device{m_Surface, m_PhysicalDevice, m_Window} {};

		~Application() {
			DEBUG("Terminating application");
			glfwTerminate();
		}

		void Run();

	private:
		void InitVulkan();

		void MainLoop();

		void DrawFrame();

		void InitRayTracing();

		void CreateRtDescriptorSet();

		void UpdateRtDescriptorSet();

		Model &LoadModel(const std::string &fileName);

		[[nodiscard]]
		VkWriteDescriptorSet MakeWrite(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t arrayElement);

		[[nodiscard]]
		VkWriteDescriptorSet MakeWrite(
		        VkDescriptorSet dstSet, uint32_t dstBinding,
		        const VkWriteDescriptorSetAccelerationStructureKHR *pAccelerationStructure, uint32_t arrayElement = 0
		);

		[[nodiscard]]
		VkWriteDescriptorSet MakeWrite(
		        VkDescriptorSet dstSet, uint32_t dstBinding, const VkDescriptorImageInfo *pImageInfo,
		        uint32_t arrayElement = 0
		);

		[[nodiscard]]
		VkDescriptorPool CreatePool(uint32_t maxSets = 1, VkDescriptorPoolCreateFlags flags = 0);

		[[nodiscard]]
		VkDescriptorSetLayout CreateDescriptorSetLayout(VkDescriptorSetLayoutCreateFlags flags);

		void AddRequiredPoolSizes(std::vector<VkDescriptorPoolSize> &poolSizes, uint32_t numSets);

		static constexpr int              WINDOW_WIDTH{800}, WINDOW_HEIGHT{600};
		static constexpr std::string_view WINDOW_TITLE{"Vulkan"};
		const std::vector<Vertex>         VERTICES{
                Vertex{{-0.5f, -0.5f, 0.f}, {1.0f, 0.0f, 0.0f}}, Vertex{{0.5f, -0.5f, 0.f}, {0.0f, 1.0f, 0.0f}},
                Vertex{{0.5f, 0.5f, 0.f}, {0.0f, 0.0f, 1.0f}}, Vertex{{-0.5f, 0.5f, 0.f}, {1.0f, 0.0f, 1.0f}}
        };
		const std::vector<uint32_t> INDICES{0, 1, 2, 2, 3, 0};

		vk::Window       m_Window;
		vk::Instance     m_Instance;
		vk::Surface      m_Surface;
		VkPhysicalDevice m_PhysicalDevice{};
		vk::Device       m_Device;

		std::vector<Model>         m_Models{};
		std::vector<ModelInstance> m_ModelInstances{};

		std::vector<VkDescriptorSetLayoutBinding> m_DescriptorSetLayoutBindings{};
		std::vector<VkDescriptorBindingFlags>     m_DescriptorBindingFlags{};
		VkDescriptorPool                          m_DescPool{};
		VkDescriptorSetLayout                     m_DescSetLayout{};
		VkDescriptorSet                           m_DescSet{};

		std::vector<VkFramebuffer>                      m_SwapChainFramebuffers{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RtProperties{
		        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		};
	};
}// namespace roing


#endif//PORTAL2RAYTRACED_APPLICATION_H
