#pragma once

struct vec2 {
	float x, y;
};

inline vec2 operator +(const vec2 a, const vec2 b) {
	vec2 tmp = {a.x + b.x, a.y + b.y};
	return tmp;
}

inline vec2 operator -(const vec2 a, const vec2 b) {
	vec2 tmp = {a.x - b.x, a.y - b.y};
	return tmp;
}

inline vec2 operator *(const float s, const vec2 v) {
	vec2 tmp = {s*v.x, s*v.y};
	return tmp;
}

inline vec2 operator *(const vec2 v, const float s) {
	return s * v;
}

inline vec2 operator /(const float s, const vec2 v) {
	vec2 tmp = {s/v.x, s/v.y};
	return tmp;
}

inline vec2 operator /(const vec2 v, const float s) {
	vec2 tmp = {v.x/s, v.y/s};
	return tmp;
}

inline vec2 operator -(const vec2 v) {
	vec2 tmp = {-v.x, -v.y};
	return tmp;
}

inline float dot(const vec2 a, const vec2 b) {
	return a.x*b.x + a.y*b.y;
}

inline float length_sqr(const vec2 v) {
	return dot(v, v);
}
