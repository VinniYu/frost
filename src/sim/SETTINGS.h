#ifndef SETTINGS_H
#define SETTINGS_H

#include <cmath>
#include <vector>
#include <iostream>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#if _WIN32
#include <GL/freeglut.h>
#elif __APPLE__
#include <GLUT/glut.h>
#elif __linux__
#include <GL/freeglut.h>
#endif

typedef double REAL;

typedef glm::vec2 vec2;
typedef glm::vec3 vec3;
typedef glm::vec4 vec4;

typedef glm::mat4 mat4;

struct AxialCoord { int q, r; };

#endif