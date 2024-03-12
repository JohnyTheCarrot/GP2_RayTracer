#ifndef PORTAL2RAYTRACED_SHADERMODULE_H
#define PORTAL2RAYTRACED_SHADERMODULE_H

#include "raii.h"
#include <vector>

namespace roing::vk {

	class Device;

	class ShaderModule final {
	public:
		ShaderModule(const Device &device, std::vector<char> &&code);

		[[nodiscard]]
		ShaderModuleHandle::pointer GetHandle() const noexcept;

	private:
		ShaderModuleHandle m_hShaderModule;
	};

}// namespace roing::vk

#endif//PORTAL2RAYTRACED_SHADERMODULE_H
