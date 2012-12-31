#include "SpriteBuffer.hpp"

#include "GL3/gl3w.h"

SpriteBuffer::SpriteBuffer() :
	vertex_count(0), index_count(0),
	tex_width(1.0f), tex_height(1.0f)
{ }

void SpriteBuffer::clear() {
	vertices.clear();
	vertex_count = 0;
}

void SpriteBuffer::append(const Sprite& spr) {
	float img_x = spr.img_x / tex_width;
	float img_w = spr.img_w / tex_width;
	float img_y = spr.img_y / tex_height;
	float img_h = spr.img_h / tex_height;

	VertexData v;

	v.pos_x = spr.x;
	v.pos_y = spr.y;
	v.tex_s = img_x;
	v.tex_t = img_y;
	vertices.push_back(v);

	v.pos_x = spr.x + spr.w;
	v.tex_s = img_x + img_w;
	vertices.push_back(v);

	v.pos_y = spr.y + spr.h;
	v.tex_t = img_y + img_h;
	vertices.push_back(v);

	v.pos_x = spr.x;
	v.tex_s = img_x;
	vertices.push_back(v);

	vertex_count += 1;
}

// Returns true if indices need to be updated
bool SpriteBuffer::generate_indices() {
	if (index_count >= vertex_count)
		return false;

	indices.reserve(vertex_count * 6);
	for (unsigned int i = index_count; i < vertex_count; ++i) {
		unsigned short base_i = i * 4;

		indices.push_back(base_i + 0);
		indices.push_back(base_i + 1);
		indices.push_back(base_i + 3);

		indices.push_back(base_i + 3);
		indices.push_back(base_i + 1);
		indices.push_back(base_i + 2);
	}

	index_count = vertex_count;
	return true;
}

void SpriteBuffer::upload() {
	if (generate_indices()) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort)*indices.size(), indices.data(), GL_STREAM_DRAW);
	}
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexData)*vertices.size(), vertices.data(), GL_STREAM_DRAW);
}

void SpriteBuffer::draw() {
	glDrawElements(GL_TRIANGLES, vertex_count * 6, GL_UNSIGNED_SHORT, nullptr);
}