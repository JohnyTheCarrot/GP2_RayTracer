#ifndef PORTAL2RAYTRACED_TEXTURE_H
#define PORTAL2RAYTRACED_TEXTURE_H

#include "Image.h"
#include "raii.h"

namespace roing::vk {

	struct Texture final {
		Image                        m_Image;
		VkDescriptorImageInfo        descriptor{};
		std::optional<SamplerHandle> m_Sampler;

		void CreateSampler(const Device &device);
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_TEXTURE_H
