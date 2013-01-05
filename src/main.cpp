#define GLFW_INCLUDE_GL3 1
#include <GL/glfw.h>
#include "GL3/gl3w.h"

#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <cstdint>
#include <array>
#include <algorithm>
#include <string>
#include "util.hpp"
#include "Fixed.hpp"
#include "SpriteBuffer.hpp"
#include "vec2.hpp"
#include "graphics_init.hpp"

std::vector<Sprite> debug_sprites;

void debugPoint(int x, int y) {
	Sprite spr;
	spr.color = makeColor(255, 0, 0, 255);
	spr.setImg(16 + 2, 16 + 2, 4, 4);
	spr.setPos(x - 2, y - 2);

	debug_sprites.push_back(spr);
}

struct Gem {
	fixed24_8 pos_x;
	fixed24_8 pos_y;

	fixed16_16 vel_x;
	fixed16_16 vel_y;

	int score_value;
	static const int MAX_VALUE = 10000;
	static const int INITIAL_VALUE = 100;

	static const int RADIUS = 8;
	static const int MERGE_SPEED = 6;
};

struct Paddle {
	fixed24_8 pos_x;
	fixed24_8 pos_y;

	fixed8_24 rotation;

	SpriteMatrix getSpriteMatrix() const {
		return SpriteMatrix().loadIdentity().rotate(rotation.toFloat());
	};
};

static const fixed24_8 PADDLE_MOVEMENT_SPEED(4);
static const fixed8_24 PADDLE_MAX_ROTATION(15);
static const fixed8_24 PADDLE_ROTATION_RATE(3);
static const fixed8_24 PADDLE_ROTATION_RETURN_RATE(1);

struct GameState {
	RandomGenerator rng;

	Paddle paddle;
	std::vector<Gem> gems;

	int score;
	int lives;

	GameState()
		: score(0), lives(5)
	{ }
};

static const int WINDOW_WIDTH = 240;
static const int WINDOW_HEIGHT = 360;

// Splits vector vel into components parallel and perpendicular to the normal
// of the plane n.
void splitVector(vec2 vel, vec2 n, vec2* out_par, vec2* out_perp) {
	vec2 par = dot(vel, n) * n;
	*out_par = par;
	*out_perp = vel - par;
}

void collideBallWithBoundary(Gem& ball) {
	// Left boundary
	if (ball.pos_x - Gem::RADIUS < 0) {
		ball.vel_x = -ball.vel_x;
		ball.pos_x = Gem::RADIUS;
	}

	// Right boundary
	if (ball.pos_x + Gem::RADIUS > WINDOW_WIDTH) {
		ball.vel_x = -ball.vel_x;
		ball.pos_x = WINDOW_WIDTH - Gem::RADIUS;
	}

	// Top boundary
	/*
	if (ball.pos_y - Gem::RADIUS < 0) {
		ball.vel_y = -ball.vel_y;
		ball.pos_y = Gem::RADIUS;
	}
	*/

	// Bottom boundary
	/*
	if (ball.pos_y + Gem::RADIUS > WINDOW_HEIGHT) {
		ball.vel_y = -ball.vel_y;
		ball.pos_y = WINDOW_HEIGHT - Gem::RADIUS;
	}
	*/
}

void collideBallWithBall(Gem& a, Gem& b) {
	vec2 dv = {(a.pos_x - b.pos_x).toFloat(), (a.pos_y - b.pos_y).toFloat()};
	float d_sqr = length_sqr(dv);

	if (d_sqr < (2*Gem::RADIUS)*(2*Gem::RADIUS)) {
		fixed16_16 rel_vel_x = a.vel_x - b.vel_x;
		fixed16_16 rel_vel_y = a.vel_y - b.vel_y;
		vec2 rel_vel = {rel_vel_x.toFloat(), rel_vel_y.toFloat()};
		float rel_speed_sqr = length_sqr(rel_vel);
		
		if (rel_speed_sqr >= Gem::MERGE_SPEED*Gem::MERGE_SPEED) {
			fixed32_0 two(2);
			a.pos_x = (a.pos_x + b.pos_x) / two;
			a.pos_y = (a.pos_y + b.pos_y) / two;

			a.vel_x = a.vel_x + b.vel_x;
			a.vel_y = a.vel_y + b.vel_y;

			a.score_value += b.score_value;

			b.pos_x = -9999;
			b.pos_y = 9999;
			b.vel_x = b.vel_y = 0;
			b.score_value = 0;
		} else {
			float d = std::sqrt(d_sqr);
			float sz = Gem::RADIUS - d / 2.0f;

			vec2 normal = dv / d;
			fixed24_8 push_back_x(sz * normal.x);
			fixed24_8 push_back_y(sz * normal.y);

			a.pos_x += push_back_x;
			a.pos_y += push_back_y;
			b.pos_x -= push_back_x;
			b.pos_y -= push_back_y;

			vec2 a_par, a_perp;
			vec2 b_par, b_perp;

			vec2 a_vel = {a.vel_x.toFloat(), a.vel_y.toFloat()};
			vec2 b_vel = {b.vel_x.toFloat(), b.vel_y.toFloat()};
			splitVector(a_vel, normal, &a_par, &a_perp);
			splitVector(b_vel, -normal, &b_par, &b_perp);

			static const float friction = 1.0f;
			static const float bounce = 0.9f;

			float A = (1.0f + bounce) / 2.0f;
			float B = (1.0f - bounce) / 2.0f;

			a_vel = A*b_par + B*a_par + friction*a_perp;
			b_vel = A*a_par + B*b_par + friction*b_perp;

			a.vel_x = fixed16_16(a_vel.x);
			a.vel_y = fixed16_16(a_vel.y);

			b.vel_x = fixed16_16(b_vel.x);
			b.vel_y = fixed16_16(b_vel.y);
		}
	}
}

// Returns the nearest point in line segment a-b to point p.
vec2 pointLineSegmentNearestPoint(vec2 p, vec2 a, vec2 b) {
	// Taken from http://stackoverflow.com/a/1501725
	const float l2 = length_sqr(b - a);
	if (l2 == 0.0f) {
		return a;
	}

	const float t = dot(p - a, b - a) / l2;
	if (t < 0.0f) {
		return a;
	} else if (t > 1.0f) {
		return b;
	} else {
		return a + t * (b - a);
	}
}

void collideBallWithPaddle(Gem& ball, const Paddle& paddle) {
	SpriteMatrix matrix = paddle.getSpriteMatrix();

	// Left sphere
	vec2 left = {-24, 0};
	// Right sphere
	vec2 right = {24, 0};

	matrix.transform(&left.x, &left.y);
	matrix.transform(&right.x, &right.y);

	static const int PADDLE_RADIUS = 8;

	fixed24_8 rel_ball_x = ball.pos_x - paddle.pos_x;
	fixed24_8 rel_ball_y = ball.pos_y - paddle.pos_y;
	vec2 rel_ball = {rel_ball_x.toFloat(), rel_ball_y.toFloat()};

	vec2 nearest_point = pointLineSegmentNearestPoint(rel_ball, left, right);
	vec2 penetration = rel_ball - nearest_point;
	float d_sqr = length_sqr(penetration);
	float r = PADDLE_RADIUS + Gem::RADIUS;
	if (d_sqr < r*r) {
		vec2 vel = {ball.vel_x.toFloat(), ball.vel_y.toFloat()};
		int score_addition = static_cast<int>(ball.score_value * (ball.vel_y.toFloat() / 128.f));
		ball.score_value = std::min(ball.score_value + std::max(score_addition, 0), Gem::MAX_VALUE);

		float d = std::sqrt(d_sqr);
		float sz = r - d;

		vec2 normal = penetration / d;
		fixed24_8 push_back_x(sz * normal.x);
		fixed24_8 push_back_y(sz * normal.y);

		ball.pos_x += push_back_x;
		ball.pos_y += push_back_y;

		vec2 par, perp;
		splitVector(vel, normal, &par, &perp);
		vel = perp - par;

		ball.vel_x = fixed16_16(vel.x);
		ball.vel_y = fixed16_16(vel.y);
	}
}

void hsvToRgb(float h, float s, float v, float* out_r, float* out_g, float* out_b) {
	// Taken from http://www.cs.rit.edu/~ncs/color/t_convert.html

	assert(h >= 0.0f && h < 360.f);
	assert(s >= 0.0f && s <= 1.0f);
	assert(v >= 0.0f && v <= 1.0f);

	h /= 60.0f;
	int i = static_cast<int>(h);
	float f = h - i;
	float p = v*(1.0f - s);
	float q = v*(1.0f - s*f);
	float t = v*(1.0f - s*(1.0f - f));

#define CASE(i, r, g, b) case i: *out_r = r; *out_g = g; *out_b = b; break
	switch (i) {
		CASE(0, v, t, p);
		CASE(1, q, v, p);
		CASE(2, p, v, t);
		CASE(3, p, q, v);
		CASE(4, t, p, v);
		CASE(5, v, p, q);
	default: assert(false);
	}
#undef CASE
}

float mapScoreToHue(int score_value) {
	score_value -= Gem::INITIAL_VALUE;

	float score_range = static_cast<float>(score_value) / (Gem::MAX_VALUE - Gem::INITIAL_VALUE);
	return std::sqrt(score_range) * 300.0f;
}

struct FontInfo {
	char first_char;
	int img_x, img_y;
	int img_w, img_h;

	FontInfo(char first_char, int img_x, int img_y, int img_w, int img_h)
		: first_char(first_char), img_x(img_x), img_y(img_y), img_w(img_w), img_h(img_h)
	{ }
};

void drawText(int x, int y, const std::string& text, SpriteBuffer& buffer, const FontInfo& font)
{
	Sprite spr;
	spr.setPos(x, y);
	spr.setImg(font.img_x, font.img_y, font.img_w, font.img_h);

	for (char c : text) {
		spr.img_x = font.img_x + (c - font.first_char) * font.img_w;
		buffer.append(spr);
		spr.x += font.img_w;
	}
}

int main() {
	if (!initWindow(WINDOW_WIDTH, WINDOW_HEIGHT)) {
		std::cerr << "Failed to initialize window.\n";
		return 1;
	}

	int tex_width, tex_height;
	GLuint main_texture = loadTexture(&tex_width, &tex_height, "graphics.png");
	assert(main_texture != 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLuint shader_program = loadShaderProgram();
	glUseProgram(shader_program);

	GLuint u_view_matrix_location = glGetUniformLocation(shader_program, "u_view_matrix");
	GLfloat view_matrix[9] = {
		2.0f/WINDOW_WIDTH,  0.0f,              -1.0f,
		0.0f,              -2.0/WINDOW_HEIGHT,  1.0f,
		0.0f,               0.0f,               1.0f
	};
	glUniformMatrix3fv(u_view_matrix_location, 1, GL_TRUE, view_matrix);

	GLuint u_texture_location = glGetUniformLocation(shader_program, "u_texture");
	glUniform1i(u_texture_location, 0);

	CHECK_GL_ERROR;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, main_texture);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	CHECK_GL_ERROR;

	SpriteBuffer sprite_buffer;
	sprite_buffer.tex_width = static_cast<float>(tex_width);
	sprite_buffer.tex_height = static_cast<float>(tex_height);

	GLuint vao_id;
	glGenVertexArrays(1, &vao_id);
	glBindVertexArray(vao_id);

	GLuint vbo_id;
	glGenBuffers(1, &vbo_id);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), reinterpret_cast<void*>(offsetof(VertexData, pos_x)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_TRUE,  sizeof(VertexData), reinterpret_cast<void*>(offsetof(VertexData, tex_s)));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(VertexData), reinterpret_cast<void*>(offsetof(VertexData, color)));
	for (int i = 0; i < 3; ++i)
		glEnableVertexAttribArray(i);

	GLuint ibo_id;
	glGenBuffers(1, &ibo_id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);

	///////////////////////////
	// Initialize game state //
	///////////////////////////
	GameState game_state;
	RandomGenerator& rng = game_state.rng;
	rng.seed(123);

	{
		Paddle& p = game_state.paddle;
		p.pos_x = WINDOW_WIDTH / 2;
		p.pos_y = WINDOW_HEIGHT - 32;
		p.rotation = 0;
	}

	Sprite paddle_spr;
	paddle_spr.setImg(0, 0, 64, 16);

	Sprite gem_spr;
	gem_spr.setImg(0, 16, 16, 16);

	static const int GEM_SPAWN_INTERVAL = 60*5;
	int gem_spawn_timer = GEM_SPAWN_INTERVAL;

	CHECK_GL_ERROR;

	////////////////////
	// Main game loop //
	////////////////////
	bool running = true;
	while (running) {
		/* Update paddle */
		{
			Paddle& paddle = game_state.paddle;

			fixed24_8 paddle_speed(0);
			fixed8_24 rotation = 0;
			if (glfwGetKey(GLFW_KEY_LEFT)) {
				paddle_speed -= PADDLE_MOVEMENT_SPEED;
				rotation -= PADDLE_ROTATION_RATE;
			}
			if (glfwGetKey(GLFW_KEY_RIGHT)) {
				paddle_speed += PADDLE_MOVEMENT_SPEED;
				rotation += PADDLE_ROTATION_RATE;
			}

			if (rotation == 0) {
				paddle.rotation = stepTowards(paddle.rotation, fixed8_24(0), PADDLE_ROTATION_RETURN_RATE);
			} else {
				paddle.rotation = clamp(-PADDLE_MAX_ROTATION, paddle.rotation + rotation, PADDLE_MAX_ROTATION);
			}
			paddle.pos_x += paddle_speed;
		}

		/* Spawn new gems */
		if (--gem_spawn_timer == 0) {
			gem_spawn_timer = GEM_SPAWN_INTERVAL;

			Gem b;
			b.pos_x = randRange(rng, WINDOW_WIDTH * 1 / 6, WINDOW_WIDTH * 5 / 6);
			b.pos_y = -10;
			b.vel_x = b.vel_y = 0;
			b.score_value = Gem::INITIAL_VALUE;

			game_state.gems.push_back(b);
		}

		/* Update balls */
		for (unsigned int i = 0; i < game_state.gems.size(); ++i) {
			Gem& ball = game_state.gems[i];

			ball.vel_y += fixed16_16(0, 1, 8);

			ball.pos_x += fixed24_8(ball.vel_x);
			ball.pos_y += fixed24_8(ball.vel_y);

			collideBallWithBoundary(ball);
			for (unsigned int j = i + 1; j < game_state.gems.size(); ++j) {
				collideBallWithBall(ball, game_state.gems[j]);
			}
			collideBallWithPaddle(ball, game_state.paddle);
		}

		/* Clean up dead gems */
		remove_if(game_state.gems, [](const Gem& gem) {
			return gem.pos_y > WINDOW_HEIGHT + 128 && gem.vel_y > 0;
		});

		/* Draw scene */
		sprite_buffer.clear();
		
		paddle_spr.setPos(game_state.paddle.pos_x.integer(), game_state.paddle.pos_y.integer());
		sprite_buffer.append(paddle_spr, game_state.paddle.getSpriteMatrix());

		for (const Gem& gem : game_state.gems) {
			gem_spr.setPos(gem.pos_x.integer() - gem_spr.img_w / 2, gem.pos_y.integer() - gem_spr.img_h / 2);
			float r, g, b;
			hsvToRgb(mapScoreToHue(gem.score_value), 1.0f, 1.0f, &r, &g, &b);
			gem_spr.color = makeColor(uint8_t(r*255 + 0.5f), uint8_t(g*255 + 0.5f), uint8_t(b*255 + 0.5f), 255);
			sprite_buffer.append(gem_spr);
		}

		// HUD
		{
			static const int HUD_X_POS = 1;
			static const int HUD_Y_POS = 1;

			Sprite hud_spr;
			hud_spr.setImg(64, 0, 29, 11);
			hud_spr.setPos(HUD_X_POS, HUD_Y_POS);
			sprite_buffer.append(hud_spr);

			hud_spr.setImg(64, 12, 29, 11);
			hud_spr.setPos(HUD_X_POS, HUD_Y_POS + 13);
			sprite_buffer.append(hud_spr);

			std::string score_text = std::to_string(game_state.score);
			FontInfo font('0', 40, 24, 8, 12);
			drawText(HUD_X_POS + 31, HUD_Y_POS, score_text, sprite_buffer, font);
		}

		/* Submit sprites */
		// More superfluous drawcalls to change the GPU into high-performance mode? Sure, why not.
		glClear(GL_COLOR_BUFFER_BIT);
		for (int i = 0; i < 1000; ++i) {
			sprite_buffer.upload();
			sprite_buffer.draw();
		}

		for (const Sprite& spr : debug_sprites) {
			sprite_buffer.append(spr);
		}
		debug_sprites.clear();

		glClear(GL_COLOR_BUFFER_BIT);
		sprite_buffer.upload();
		sprite_buffer.draw();

		glfwSwapBuffers();
		running = running && glfwGetWindowParam(GLFW_OPENED);

		CHECK_GL_ERROR;
	}

	glfwCloseWindow();
	glfwTerminate();
}
