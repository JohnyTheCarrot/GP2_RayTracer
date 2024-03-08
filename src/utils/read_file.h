#ifndef PORTAL2RAYTRACED_READ_FILE_H
#define PORTAL2RAYTRACED_READ_FILE_H

#include <fstream>
#include <string_view>
#include <vector>

namespace roing::vk {
	[[nodiscard]]
	static std::vector<char> ReadFile(std::string_view fileName) {
		std::ifstream file{fileName.data(), std::ios::ate | std::ios::binary};

		if (!file.is_open()) {
			throw std::runtime_error{"Failed to open file: "};
		}

		size_t            fileSize{static_cast<size_t>(file.tellg())};
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

		file.close();

		return buffer;
	}
}// namespace roing::vk
#endif//PORTAL2RAYTRACED_READ_FILE_H
