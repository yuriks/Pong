#pragma once
#include "GL3/gl3.h"
#include <vector>

struct VertexData {
	GLfloat pos_x, pos_y;
	GLfloat tex_s, tex_t;
};

struct Sprite {
	float x, y;
	float w, h;
	float img_x, img_y;
	float img_h, img_w;
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

	// Returns true if indices need to be updated
	bool generate_indices();

	void upload();
	void draw();
};
