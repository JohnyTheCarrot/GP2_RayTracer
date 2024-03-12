#ifndef PORTAL2RAYTRACED_DESCRIPTORSET_H
#define PORTAL2RAYTRACED_DESCRIPTORSET_H

#include "DescriptorSetLayout.h"
#include "raii.h"
#include <cstdint>

namespace roing::vk {

	class Device;

	class DescriptorPool;

	class DescriptorSet final {
	public:
		DescriptorSet(
		        const Device &device, const DescriptorPool &descriptorPool,
		        const DescriptorSetLayout &descriptorSetLayout
		);

		[[nodiscard]]
		DescriptorSetHandle::pointer GetHandle() const noexcept {
			return m_hDescriptorSet.get();
		}

	private:
		DescriptorSetHandle m_hDescriptorSet;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_DESCRIPTORSET_H
