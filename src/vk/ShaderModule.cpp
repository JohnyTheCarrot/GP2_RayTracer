#include "ShaderModule.h"
#include "Device.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	ShaderModule::ShaderModule(const Device &device, std::vector<char> &&code)
	    : m_hShaderModule{VK_NULL_HANDLE, ShaderModuleDestroyer{device.GetHandle()}} {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

		VkShaderModule shaderModule{};
		if (const VkResult result{vkCreateShaderModule(device.GetHandle(), &createInfo, nullptr, &shaderModule)};
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create shader module: "} + string_VkResult(result)};
		}

		m_hShaderModule.reset(shaderModule);
	}

	ShaderModuleHandle::pointer ShaderModule::GetHandle() const noexcept {
		return m_hShaderModule.get();
	}
}// namespace roing::vk
