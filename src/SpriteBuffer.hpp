#pragma once
#include "GL3/gl3.h"
#include <vector>

struct VertexData {
	GLfloat pos_x, pos_y;
	GLfloat tex_s, tex_t;
};

struct Sprite {
	float x, y;
	float img_x, img_y;
	float img_h, img_w;
};

struct SpriteMatrix {
	GLfloat m[4]; // Row-major storage

	SpriteMatrix& loadIdentity();
	SpriteMatrix& multiply(const SpriteMatrix& l);
	SpriteMatrix& rotate(float degrees);
	SpriteMatrix& scale(float x, float y);
	SpriteMatrix& shear(float x, float y);
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
