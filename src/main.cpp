#define GLFW_INCLUDE_GL3 1
#include <GL/glfw.h>
#include "GL3/gl3w.h"

#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <cstdint>
#include "Fixed.hpp"

#define CHECK_GL_ERROR assert(glGetError() == GL_NO_ERROR)

GLuint loadMainTexture(int* width, int* height) {
	GLuint main_texture;
	glGenTextures(1, &main_texture);

	glBindTexture(GL_TEXTURE_2D, main_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	int comp;
	unsigned char* data = stbi_load("graphics.png", width, height, &comp, 4);
	if (data == nullptr)
		return 0;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, *width, *height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	return main_texture;
}

GLuint loadShader(const char* shader_src, GLenum shader_type) {
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &shader_src, nullptr);
	glCompileShader(shader);

	GLint compile_result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_result);

	if (compile_result == GL_FALSE) {
		GLint log_size;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);

		std::vector<char> log_buf(log_size);
		glGetShaderInfoLog(shader, log_buf.size(), nullptr, log_buf.data());

		std::cerr << "Error compiling shader:\n";
		std::cerr << log_buf.data() << std::flush;

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

std::string loadTextFile(const std::string& filename) {
	std::string ret;
	
	std::ifstream f(filename);
	if (f) {
		f.seekg(0, std::ios::end);
		ret.resize(static_cast<size_t>(f.tellg()));
		f.seekg(0);
		f.read(&ret[0], ret.size());
	}

	return ret;
}

GLuint loadShaderProgram() {
	std::string vertex_shader_src = loadTextFile("vertex_shader.glsl");
	assert(!vertex_shader_src.empty());
	std::string fragment_shader_src = loadTextFile("fragment_shader.glsl");
	assert(!fragment_shader_src.empty());

	GLuint vertex_shader = loadShader(vertex_shader_src.c_str(), GL_VERTEX_SHADER);
	assert(vertex_shader != 0);
	GLuint fragment_shader = loadShader(fragment_shader_src.c_str(), GL_FRAGMENT_SHADER);
	assert(fragment_shader != 0);

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	GLint link_result;
	glGetProgramiv(program, GL_LINK_STATUS, &link_result);

	if (link_result == GL_FALSE) {
		GLint log_size;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_size);

		std::vector<char> log_buf(log_size);
		glGetProgramInfoLog(program, log_buf.size(), nullptr, log_buf.data());

		std::cerr << "Error linking shader program:\n";
		std::cerr << log_buf.data() << std::flush;

		glDeleteProgram(program);
		return 0;
	}

	return program;
}

void APIENTRY debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam)
{
	if ((type != GL_DEBUG_TYPE_PERFORMANCE_ARB && type != GL_DEBUG_TYPE_OTHER_ARB) || severity != GL_DEBUG_SEVERITY_LOW_ARB)
		std::cerr << message << std::endl;
	if ((type != GL_DEBUG_TYPE_PERFORMANCE_ARB && type != GL_DEBUG_TYPE_OTHER_ARB) || severity == GL_DEBUG_SEVERITY_HIGH_ARB)
		__debugbreak(); // Breakpoint
}

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

	SpriteBuffer() :
		vertex_count(0), index_count(0),
		tex_width(1.0f), tex_height(1.0f)
	{ }

	void clear() {
		vertices.clear();
		vertex_count = 0;
	}

	void append(const Sprite& spr) {
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
	bool generate_indices() {
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

	void upload() {
		if (generate_indices()) {
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort)*indices.size(), indices.data(), GL_STREAM_DRAW);
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(VertexData)*vertices.size(), vertices.data(), GL_STREAM_DRAW);
	}

	void draw() {
		glDrawElements(GL_TRIANGLES, vertex_count * 6, GL_UNSIGNED_SHORT, nullptr);
	}
};

struct GameState {
	fixed24_8 ball_pos_x;
	fixed24_8 ball_pos_y;

	fixed24_8 ball_vel_x;
	fixed24_8 ball_vel_y;
};

int main() {
	if (!glfwInit()) {
		return 1;
	}

	static const int WINDOW_WIDTH = 320;
	static const int WINDOW_HEIGHT = 240;

	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
	if (!glfwOpenWindow(WINDOW_WIDTH, WINDOW_HEIGHT, 8, 8, 8, 0, 0, 8, GLFW_WINDOW)) {
		return 1;
	}

	if (gl3wInit()) {
		return 1;
	}

	if (!gl3wIsSupported(3, 3)) {
		return 1;
	}

	glfwSwapInterval(1);
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	if (glDebugMessageCallbackARB) {
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);
		glDebugMessageCallbackARB(debug_callback, 0);
	}

	int tex_width, tex_height;
	GLuint main_texture = loadMainTexture(&tex_width, &tex_height);
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

	Sprite tmp_spr;
	tmp_spr.img_x = tmp_spr.img_y = 0.0f;
	tmp_spr.img_w = tmp_spr.img_h = 64.0f;
	tmp_spr.w = tmp_spr.h = 64.0f;

	tmp_spr.x = 200.0f;
	tmp_spr.y = 100.0f;
	sprite_buffer.append(tmp_spr);
	tmp_spr.x = tmp_spr.y = -0.1f;
	sprite_buffer.append(tmp_spr);

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

	static const int BALL_RADIUS = 8;

	GameState game_state;
	game_state.ball_pos_x = WINDOW_WIDTH / 2;
	game_state.ball_pos_y = WINDOW_HEIGHT / 2;
	game_state.ball_vel_x = 2;
	game_state.ball_vel_y = -1;

	Sprite ball_spr;
	ball_spr.w = ball_spr.h = ball_spr.img_w = ball_spr.img_h = 16;
	ball_spr.img_x = 16; ball_spr.img_y = 0;

	CHECK_GL_ERROR;

	bool running = true;
	while (running) {
		glClear(GL_COLOR_BUFFER_BIT);
		sprite_buffer.clear();

		game_state.ball_vel_y += fixed24_8(0, 1, 8);

		game_state.ball_pos_x += game_state.ball_vel_x;
		game_state.ball_pos_y += game_state.ball_vel_y;

		if (game_state.ball_pos_x - BALL_RADIUS < 0) {
			game_state.ball_vel_x = -game_state.ball_vel_x;
			game_state.ball_pos_x = BALL_RADIUS;
		}

		if (game_state.ball_pos_x + BALL_RADIUS > WINDOW_WIDTH) {
			game_state.ball_vel_x = -game_state.ball_vel_x;
			game_state.ball_pos_x = WINDOW_WIDTH - BALL_RADIUS;
		}

		if (game_state.ball_pos_y - BALL_RADIUS < 0) {
			game_state.ball_vel_y = -game_state.ball_vel_y;
			game_state.ball_pos_y = BALL_RADIUS;
		}

		if (game_state.ball_pos_y + BALL_RADIUS > WINDOW_HEIGHT) {
			game_state.ball_vel_y = -game_state.ball_vel_y;
			game_state.ball_pos_y = WINDOW_HEIGHT - BALL_RADIUS;
		}

		ball_spr.x = static_cast<float>(game_state.ball_pos_x.integer()) - ball_spr.w / 2;
		ball_spr.y = static_cast<float>(game_state.ball_pos_y.integer()) - ball_spr.h / 2;
		sprite_buffer.append(ball_spr);

		sprite_buffer.upload();
		sprite_buffer.draw();

		glfwSwapBuffers();
		running = running && glfwGetWindowParam(GLFW_OPENED);

		CHECK_GL_ERROR;
	}

	glfwCloseWindow();
	glfwTerminate();
}
