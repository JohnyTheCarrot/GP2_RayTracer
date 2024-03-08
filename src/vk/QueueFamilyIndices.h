#ifndef PORTAL2RAYTRACED_QUEUEFAMILYINDICES_H
#define PORTAL2RAYTRACED_QUEUEFAMILYINDICES_H

#include <cstdint>
#include <optional>

namespace roing::vk {
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily{};
		std::optional<uint32_t> presentFamily{};

		[[nodiscard]]
		bool IsComplete() const noexcept {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};
}// namespace roing::vk

#endif//PORTAL2RAYTRACED_QUEUEFAMILYINDICES_H
