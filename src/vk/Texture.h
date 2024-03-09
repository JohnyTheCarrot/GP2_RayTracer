#ifndef PORTAL2RAYTRACED_TEXTURE_H
#define PORTAL2RAYTRACED_TEXTURE_H

#include "Image.h"

namespace roing::vk {

	struct Texture final {
		Image                 m_Image{};
		VkDescriptorImageInfo descriptor{};
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_TEXTURE_H
