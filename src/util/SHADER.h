#pragma once

#include <GL/glew.h>

#if _WIN32
#include <GL/freeglut.h>
#elif __APPLE__
#include <GLUT/glut.h>
#elif __linux__
#include <GL/freeglut.h>
#endif

GLuint createComputeProgram(const char* filepath);
GLuint createRenderProgram(const char* vertexPath, const char* fragmentPath);
