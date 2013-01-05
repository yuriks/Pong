#include "SpriteBuffer.hpp"

#include "GL3/gl3w.h"

SpriteMatrix& SpriteMatrix::loadIdentity() {
	m[0] = m[3] = 1.0f;
	m[1] = m[2] = 0.0f;

	return *this;
}

SpriteMatrix& SpriteMatrix::multiply(const SpriteMatrix& l) {
	SpriteMatrix r = *this;

	m[0] = l.m[0]*r.m[0] + l.m[1]*r.m[2];
	m[1] = l.m[0]*r.m[1] + l.m[1]*r.m[3];
	m[2] = l.m[2]*r.m[0] + l.m[3]*r.m[2];
	m[3] = l.m[2]*r.m[1] + l.m[3]*r.m[3];

	return *this;
}

static const float DOUBLE_PI = 6.283185482025146484375f;

SpriteMatrix& SpriteMatrix::rotate(float degrees) {
	float t = degrees / 360.0f * DOUBLE_PI;

	float sin_t = std::sin(t);
	float cos_t = std::cos(t);

	SpriteMatrix rotate_m = {{
		cos_t, -sin_t,
		sin_t, cos_t
	}};

	return multiply(rotate_m);
}

SpriteMatrix& SpriteMatrix::scale(float x, float y) {
	m[0] *= x;
	m[1] *= x;
	m[2] *= y;
	m[3] *= y;

	return *this;
}

SpriteMatrix& SpriteMatrix::shear(float x, float y) {
	SpriteMatrix r = *this;

	m[0] = r.m[0] + x*r.m[2];
	m[1] = r.m[1] + x*r.m[3];
	m[2] = y*r.m[0] + r.m[2];
	m[3] = y*r.m[1] + r.m[3];

	return *this;
}

void SpriteMatrix::transform(float* x, float* y) {
	float m0x = m[0] * *x;
	float m1y = m[1] * *y;
	float m2x = m[2] * *x;
	float m3y = m[3] * *y;

	*x = m0x + m1y;
	*y = m2x + m3y;
}

///////////////////////////////////////////////////////////

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
	v.color = spr.color;

	v.pos_x = static_cast<float>(spr.x);
	v.pos_y = static_cast<float>(spr.y);
	v.tex_s = img_x;
	v.tex_t = img_y;
	vertices.push_back(v);

	v.pos_x = static_cast<float>(spr.x + spr.img_w);
	v.tex_s = img_x + img_w;
	vertices.push_back(v);

	v.pos_y = static_cast<float>(spr.y + spr.img_h);
	v.tex_t = img_y + img_h;
	vertices.push_back(v);

	v.pos_x = static_cast<float>(spr.x);
	v.tex_s = img_x;
	vertices.push_back(v);

	vertex_count += 1;
}

void SpriteBuffer::append(const Sprite& spr, const SpriteMatrix& matrix) {
	float img_x = spr.img_x / tex_width;
	float img_w = spr.img_w / tex_width;
	float img_y = spr.img_y / tex_height;
	float img_h = spr.img_h / tex_height;

	VertexData v;
	v.color = spr.color;

	float x = spr.img_w / 2.0f;
	float y = spr.img_h / 2.0f;

	float m0x = matrix.m[0] * x;
	float m1y = matrix.m[1] * y;
	float m2x = matrix.m[2] * x;
	float m3y = matrix.m[3] * y;

	v.pos_x = spr.x - m0x - m1y;
	v.pos_y = spr.y - m2x - m3y;
	v.tex_s = img_x;
	v.tex_t = img_y;
	vertices.push_back(v);

	v.pos_x = spr.x + m0x - m1y;
	v.pos_y = spr.y + m2x - m3y;
	v.tex_s = img_x + img_w;
	vertices.push_back(v);

	v.pos_x = spr.x + m0x + m1y;
	v.pos_y = spr.y + m2x + m3y;
	v.tex_t = img_y + img_h;
	vertices.push_back(v);

	v.pos_x = spr.x - m0x + m1y;
	v.pos_y = spr.y - m2x + m3y;
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