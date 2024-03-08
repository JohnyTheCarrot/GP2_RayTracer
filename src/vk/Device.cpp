#include "Device.h"
#include "Instance.h"
#include "PhysicalDeviceUtils.h"
#include "QueueFamilyIndices.h"
#include "Window.h"
#include <set>
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	Device::~Device() noexcept {
		if (m_Device == VK_NULL_HANDLE)
			return;

		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

		vkDestroyDevice(m_Device, nullptr);
	}

	Device::Device(const Surface &surface, VkPhysicalDevice physicalDevice) {
		QueueFamilyIndices indices{physical_device::FindQueueFamilies(surface, physicalDevice)};

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
		// todo: use unordered_set?
		std::set<uint32_t> uniqueQueueFamilies{indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.f;
		for (uint32_t queueFamily: uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount       = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.emplace_back(queueCreateInfo);
		}

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
		        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
		};

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
		        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
		};

		rtPipelineFeature.pNext = &accelFeature;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = queueCreateInfos.size();
		createInfo.pQueueCreateInfos    = queueCreateInfos.data();
		createInfo.pEnabledFeatures     = nullptr;
		createInfo.pNext                = &rtPipelineFeature;

		if (Instance::VALIDATION_LAYERS_ENABLED) {
			createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
			createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
		} else {
			createInfo.enabledLayerCount = 0;
		}

		createInfo.enabledExtensionCount   = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
		createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

		if (const VkResult result{vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_Device)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create logical device: "} + string_VkResult(result)};
		}

		vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
	}

	void Device::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool        = m_CommandPool;
		allocateInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(m_Device, &allocateInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size      = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers    = &commandBuffer;

		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_GraphicsQueue);

		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
	}

	void Device::QueueSubmit(uint32_t submitCount, const VkSubmitInfo *pSubmitInfo, VkFence fence) {
		if (const VkResult result{vkQueueSubmit(m_GraphicsQueue, submitCount, pSubmitInfo, fence)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to submit draw command buffer: "} + string_VkResult(result)};
		}
	}

	bool Device::QueuePresent(const VkPresentInfoKHR *pPresentInfo, Window &window) {
		if (const VkResult result{vkQueuePresentKHR(m_PresentQueue, pPresentInfo)}; result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.PollWasResized()) {
				return true;
			}

			throw std::runtime_error{std::string{"Failed to present swap chain image: "} + string_VkResult(result)};
		}

		return false;
	}

	void Device::CreateCommandPool(const Surface &surface, VkPhysicalDevice physicalDevice) {
		QueueFamilyIndices queueFamilyIndices{vk::physical_device::FindQueueFamilies(surface, physicalDevice)};

		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (const VkResult result{vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create command pool: "} + string_VkResult(result)};
		}
	}

	void Device::CreateSwapchain(
	        const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice, Swapchain *pSwapchain
	) {
		*pSwapchain = Swapchain{this, window, surface, physicalDevice};
	}

	void Device::RecreateSwapchain(
	        const Window &window, const Surface &surface, VkPhysicalDevice physicalDevice, Swapchain *pSwapchain
	) {
		int width, height;
		glfwGetFramebufferSize(window.Get(), &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window.Get(), &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(m_Device);

		*pSwapchain = Swapchain{this, window, surface, physicalDevice};
	}

	void Device::CreateBuffer(
	        VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
	        VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory
	) const {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size        = size;
		bufferInfo.usage       = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (const VkResult result{vkCreateBuffer(GetHandle(), &bufferInfo, nullptr, &buffer)}; result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create buffer: "} + string_VkResult(result)};
		}

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(GetHandle(), buffer, &memoryRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex =
		        FindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties);

		if (const VkResult result{vkAllocateMemory(GetHandle(), &memoryAllocateInfo, nullptr, &bufferMemory)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to allocate memory: "} + string_VkResult(result)};
		}

		vkBindBufferMemory(GetHandle(), buffer, bufferMemory, 0);
	}

	uint32_t
	Device::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

		//TODO: prefer vram resource
		for (uint32_t i{0}; i < memoryProperties.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error{"Failed to find a suitable memory type"};
	}
}// namespace roing::vk
