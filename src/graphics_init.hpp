#pragma

#define GLFW_INCLUDE_GL3 1
#include <GL/glfw.h>
#include "GL3/gl3w.h"

#include <cassert>

#define CHECK_GL_ERROR assert(glGetError() == GL_NO_ERROR)

GLuint loadTexture(int* width, int* height, const char* filename);
GLuint loadShader(const char* shader_src, GLenum shader_type);
GLuint loadShaderProgram();
bool initWindow(int width, int height);
