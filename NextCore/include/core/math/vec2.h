#pragma once

#include "core/core.h"
#include <math.h>
#include <glm/vec2.hpp>

struct vec2 {
	float x;
	float y;

	vec2() : x(0.0f), y(0.0f) {}
	vec2(float x, float y) : x(x), y(y) {}
	vec2(float x) : x(x), y(x) {}

	vec2 operator+(vec2 other) {
		return { x + other.x, y + other.y };
	}

	vec2 operator-(vec2 other) {
		return { x - other.x, y - other.y };
	}

	vec2 operator*(vec2 other) {
		return { x * other.x, y * other.y };
	}
	
	vec2 operator/(vec2 other) {
		return { x * other.x, y * other.y };
	}

	void operator+=(vec2 other) {
		*this = *this + other;
	}

	void operator-=(vec2 other) {
		*this = *this - other;
	}

	void operator*=(vec2 other) {
		*this = *this * other;
	}

	void operator/=(vec2 other) {
		*this = *this * other;
	}

	operator glm::vec2() { return glm::vec2(x, y);  }
};

inline float length(vec2 v) {
	return sqrtf(v.x*v.x + v.y*v.y);
}

inline float dot(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}

inline vec2 normalize(vec2 v) {
	return v / length(v);
}
