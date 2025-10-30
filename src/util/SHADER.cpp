#include "SHADER.h"

#include <fstream>
#include <sstream>
#include <iostream>

using namespace std;

GLuint createComputeProgram(const char* filepath)
{
    // 1. read the shader source from file
    ifstream file(filepath);
    if (!file.is_open()) {
        cerr << "Failed to open shader file: " << filepath << endl;
        return 0;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    string sourceStr = buffer.str();
    const char* source = sourceStr.c_str();

    // 2. create the compute shader object
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // 3. check for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        cerr << "ERROR::COMPUTE_SHADER::COMPILATION_FAILED: " << filepath << "\n"  << infoLog << endl;
    }

    // 4. create program and link shader
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    // 5. check linking errors
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(shader);

    return program;
}

GLuint createRenderProgram(const char* vertexPath, const char* fragmentPath)
{
    // helper function to read a file into a string
    auto readFile = [](const char* path) -> string {
        ifstream file(path);
        if (!file.is_open()) {
            cerr << "Failed to open shader file: " << path << endl;
            return "";
        }
        stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    };

    // 1. read shader sources
    string vertexSourceStr = readFile(vertexPath);
    string fragmentSourceStr = readFile(fragmentPath);
    const char* vertexSource = vertexSourceStr.c_str();
    const char* fragmentSource = fragmentSourceStr.c_str();

    if (vertexSourceStr.empty() || fragmentSourceStr.empty()) {
        cerr << "Shader source missing." << endl;
        return 0;
    }

    // 2. compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cerr << "ERROR::VERTEX_SHADER::COMPILATION_FAILED\n" << infoLog << endl;
    }

    // 3. compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cerr << "ERROR::FRAGMENT_SHADER::COMPILATION_FAILED\n" << infoLog << endl;
    }

    // 4. link shaders into a program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cerr << "ERROR::SHADER_PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}