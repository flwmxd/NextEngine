#pragma once

#include "core/core.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <limits>

struct AABB {
	glm::vec3 min = glm::vec3(FLT_MAX);
	glm::vec3 max = glm::vec3(-FLT_MAX);

	void ENGINE_API update(const glm::vec3&);

	AABB ENGINE_API apply(const glm::mat4&);
	void ENGINE_API update_aabb(AABB&);

	inline glm::vec3 operator[](int i) const { return (&min)[i]; };
	inline glm::vec3 centroid() { return (min + max) / 2.0f; };
	inline glm::vec3 size() { return max - min; };
};

void ENGINE_API aabb_to_verts(AABB* self, glm::vec4* verts);
