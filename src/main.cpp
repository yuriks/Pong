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
#include "util.hpp"
#include "Fixed.hpp"
#include "SpriteBuffer.hpp"
#include "vec2.hpp"
#include "graphics_init.hpp"

std::vector<Sprite> debug_sprites;

void debugPoint(int x, int y) {
	Sprite spr;
	spr.img_w = spr.img_h = 4;
	spr.img_x = 16 + 2;
	spr.img_y = 16 + 2;
	spr.x = static_cast<float>(x - 2);
	spr.y = static_cast<float>(y - 2);

	debug_sprites.push_back(spr);
}

struct Ball {
	fixed24_8 pos_x;
	fixed24_8 pos_y;

	fixed16_16 vel_x;
	fixed16_16 vel_y;

	static const int RADIUS = 8;
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
	std::vector<Ball> balls;
};

static const int WINDOW_WIDTH = 320;
static const int WINDOW_HEIGHT = 240;

// Splits vector vel into components parallel and perpendicular to the normal
// of the plane n.
void splitVector(vec2 vel, vec2 n, vec2* out_par, vec2* out_perp) {
	vec2 par = dot(vel, n) * n;
	*out_par = par;
	*out_perp = vel - par;
}

void collideBallWithBoundary(Ball& ball) {
	// Left boundary
	if (ball.pos_x - Ball::RADIUS < 0) {
		ball.vel_x = -ball.vel_x;
		ball.pos_x = Ball::RADIUS;
	}

	// Right boundary
	if (ball.pos_x + Ball::RADIUS > WINDOW_WIDTH) {
		ball.vel_x = -ball.vel_x;
		ball.pos_x = WINDOW_WIDTH - Ball::RADIUS;
	}

	// Top boundary
	/*
	if (ball.pos_y - Ball::RADIUS < 0) {
		ball.vel_y = -ball.vel_y;
		ball.pos_y = Ball::RADIUS;
	}
	*/

	// Bottom boundary
	if (ball.pos_y + Ball::RADIUS > WINDOW_HEIGHT) {
		ball.vel_y = -ball.vel_y;
		ball.pos_y = WINDOW_HEIGHT - Ball::RADIUS;
	}
}

void collideBallWithBall(Ball& a, Ball& b) {
	vec2 dv = {(a.pos_x - b.pos_x).toFloat(), (a.pos_y - b.pos_y).toFloat()};
	float d_sqr = length_sqr(dv);

	if (d_sqr < (2*Ball::RADIUS)*(2*Ball::RADIUS)) {
		float d = std::sqrt(d_sqr);
		float sz = Ball::RADIUS - d / 2.0f;
		fixed24_8 push_back_x(sz * (dv.x/d));
		fixed24_8 push_back_y(sz * (dv.y/d));

		a.pos_x += push_back_x;
		a.pos_y += push_back_y;
		b.pos_x -= push_back_x;
		b.pos_y -= push_back_y;

		dv = dv / d;

		vec2 a_par, a_perp;
		vec2 b_par, b_perp;

		vec2 a_vel = {a.vel_x.toFloat(), a.vel_y.toFloat()};
		vec2 b_vel = {b.vel_x.toFloat(), b.vel_y.toFloat()};
		splitVector(a_vel, dv, &a_par, &a_perp);
		splitVector(b_vel, -dv, &b_par, &b_perp);

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

void collideBallWithPaddle(Ball& ball, const Paddle& paddle) {
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
	float r = PADDLE_RADIUS + Ball::RADIUS;
	if (d_sqr < r*r) {
		float d = std::sqrt(d_sqr);
		float sz = r - d;

		vec2 normal = penetration / d;
		fixed24_8 push_back_x(sz * normal.x);
		fixed24_8 push_back_y(sz * normal.y);

		ball.pos_x += push_back_x;
		ball.pos_y += push_back_y;

		vec2 par, perp;
		vec2 vel = {ball.vel_x.toFloat(), ball.vel_y.toFloat()};
		splitVector(vel, normal, &par, &perp);
		vel = perp - par;

		ball.vel_x = fixed16_16(vel.x);
		ball.vel_y = fixed16_16(vel.y);
	}
}

int main() {
	if (!initWindow(WINDOW_WIDTH, WINDOW_HEIGHT)) {
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
	for (int i = 0; i < 2; ++i)
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

	for (unsigned int i = 0; i < game_state.balls.size(); ++i) {
		Ball& ball = game_state.balls[i];

		ball.pos_x = (i + 1) * WINDOW_WIDTH / (game_state.balls.size() + 1);
		ball.pos_y = WINDOW_HEIGHT / 2;

		ball.vel_x = fixed16_16(0, randRange(rng, 1, 2048), 1024);
		if (randBool(rng)) ball.vel_x = -ball.vel_x;
		ball.vel_y = fixed16_16(0, randRange(rng, 1, 4096), 1024);
		if (randBool(rng)) ball.vel_y = -ball.vel_y;
	}

	Sprite paddle_spr;
	paddle_spr.img_w = 64;
	paddle_spr.img_h = 16;
	paddle_spr.img_x = paddle_spr.img_y = 0;

	Sprite ball_spr;
	ball_spr.img_w = ball_spr.img_h = 16;
	ball_spr.img_x = 0; ball_spr.img_y = 16;

	static const int GEM_SPAWN_INTERVAL = 60*5;
	int gem_spawn_timer = GEM_SPAWN_INTERVAL;

	CHECK_GL_ERROR;

	////////////////////
	// Main game loop //
	////////////////////
	bool running = true;
	while (running) {
		sprite_buffer.clear();

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

			paddle_spr.x = static_cast<float>(paddle.pos_x.integer());
			paddle_spr.y = static_cast<float>(paddle.pos_y.integer());

			sprite_buffer.append(paddle_spr, paddle.getSpriteMatrix());
		}

		if (--gem_spawn_timer == 0) {
			gem_spawn_timer = GEM_SPAWN_INTERVAL;

			Ball b;
			b.pos_x = randRange(rng, WINDOW_WIDTH * 1 / 6, WINDOW_WIDTH * 5 / 6);
			b.pos_y = -10;
			b.vel_x = b.vel_y = 0;

			game_state.balls.push_back(b);
		}

		/* Update balls */
		for (unsigned int i = 0; i < game_state.balls.size(); ++i) {
			Ball& ball = game_state.balls[i];

			ball.vel_y += fixed16_16(0, 1, 8);

			ball.pos_x += fixed24_8(ball.vel_x);
			ball.pos_y += fixed24_8(ball.vel_y);

			collideBallWithBoundary(ball);
			for (unsigned int j = i + 1; j < game_state.balls.size(); ++j) {
				collideBallWithBall(ball, game_state.balls[j]);
			}
			collideBallWithPaddle(ball, game_state.paddle);

			ball_spr.x = static_cast<float>(ball.pos_x.integer()) - ball_spr.img_w / 2;
			ball_spr.y = static_cast<float>(ball.pos_y.integer()) - ball_spr.img_h / 2;
			sprite_buffer.append(ball_spr);
		}

		/* Draw scene */
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
