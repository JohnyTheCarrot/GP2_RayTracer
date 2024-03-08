#ifndef PORTAL2RAYTRACED_TOTRANSFORMMATRIXKHR_H
#define PORTAL2RAYTRACED_TOTRANSFORMMATRIXKHR_H

namespace roing::utils {
	inline VkTransformMatrixKHR ToTransformMatrixKHR(glm::mat4 matrix) {
		glm::mat4            temp{glm::transpose(matrix)};
		VkTransformMatrixKHR result{};
		memcpy(&result, &temp, sizeof(VkTransformMatrixKHR));
		return result;
	}
}// namespace roing::utils

#endif//PORTAL2RAYTRACED_TOTRANSFORMMATRIXKHR_H
