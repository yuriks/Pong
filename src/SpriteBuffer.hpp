#pragma once
#include "GL3/gl3.h"
#include <vector>
#include <cstdint>
#include <array>

struct VertexData {
	GLfloat pos_x, pos_y;
	GLfloat tex_s, tex_t;
	std::array<GLubyte, 4> color;
};

typedef std::array<uint8_t, 4> Color;
inline Color makeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	Color c = {{r, g, b, a}};
	return c;
}

struct Sprite {
	float x, y;
	float img_x, img_y;
	float img_h, img_w;
	Color color;
};

struct SpriteMatrix {
	GLfloat m[4]; // Row-major storage

	SpriteMatrix& loadIdentity();
	SpriteMatrix& multiply(const SpriteMatrix& l);
	SpriteMatrix& rotate(float degrees);
	SpriteMatrix& scale(float x, float y);
	SpriteMatrix& shear(float x, float y);

	void transform(float* x, float* y);
};

struct SpriteBuffer {
	std::vector<VertexData> vertices;
	std::vector<GLushort> indices;

	unsigned int vertex_count;
	unsigned int index_count;

	float tex_width;
	float tex_height;

	SpriteBuffer();

	void clear();
	void append(const Sprite& spr);
	// Careful: spr position gives center of sprite, not top-left
	void append(const Sprite& spr, const SpriteMatrix& matrix);

	// Returns true if indices need to be updated
	bool generate_indices();

	void upload();
	void draw();
};
