#include "Texture.h"
#include "Device.h"
#include <vulkan/vk_enum_string_helper.h>

namespace roing::vk {
	void Texture::CreateSampler(const Device &device) {
		// NOTE: if I need more samplers this needs to be generalized into a Sampler wrapper with a ctor
		VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		samplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.anisotropyEnable        = VK_FALSE;
		samplerCreateInfo.maxAnisotropy           = 1.f;
		samplerCreateInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable           = VK_FALSE;
		samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		if (const VkResult result{vkCreateSampler(device.GetHandle(), &samplerCreateInfo, nullptr, &descriptor.sampler)
		    };
		    result != VK_SUCCESS) {
			throw std::runtime_error{std::string{"Failed to create sampler: "} + string_VkResult(result)};
		}

		m_Sampler->reset(descriptor.sampler);
	}
}// namespace roing::vk
