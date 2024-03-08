#ifndef PORTAL2RAYTRACED_MODELINSTANCE_H
#define PORTAL2RAYTRACED_MODELINSTANCE_H

#include <glm\glm.hpp>

namespace roing {

	class Model;

	struct ModelInstance final {
		const Model *m_Model{};
		glm::mat4    m_ModelMatrix{};
		uint32_t     objIndex{};
	};

}// namespace roing

#endif//PORTAL2RAYTRACED_MODELINSTANCE_H
