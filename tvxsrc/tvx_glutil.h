#pragma once
#include "tvxutil.h"

static constexpr int MAX_SHADER_SZ = 65535;

namespace tvx {

  float dynamic_normalize(float value, float min, float max)
  {
      return (value - min) / (max - min);
  }

  glm::vec3 dynamic_normalize(glm::vec3 value, glm::vec3 min, glm::vec3 max)
  {
      return (value - min) / (max - min);
  }

	GLuint program_from_string(const char *vsString, const char *fsString) {

    unsigned int vertex, fragment;
		int success;
		char infoLog[512];

		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vsString, nullptr);
		glCompileShader(vertex);
		glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
		if(!success)
		{
			glGetShaderInfoLog(vertex, 512, nullptr, infoLog);
			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
		}

		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fsString, nullptr);
		glCompileShader(fragment);
		glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
		if(!success)
		{
			glGetShaderInfoLog(fragment, 512, nullptr, infoLog);
			std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
		}

		GLuint shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertex);
		glAttachShader(shaderProgram, fragment);
		glLinkProgram(shaderProgram);
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
		if(!success)
		{
			glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n\n\n" << infoLog << std::endl;
		}

		glDeleteShader(vertex);
		glDeleteShader(fragment);

		return shaderProgram;
	}

	GLuint program_from_file(const char *vsFilename, const char *fsFilename) {
		std::string vertexCode = load_file_as_string(vsFilename);
		std::string fragmentCode = load_file_as_string(fsFilename);

		return program_from_string(vertexCode.c_str(), fragmentCode.c_str());
	}

	void reload_program(GLuint *program, const char *vsFilename, const char *fsFilename) {
		if (!program || !vsFilename || !fsFilename) {
			SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Bad arguments passed to shaderReloadFile");
		}
		GLuint reloadedProgram = program_from_file(vsFilename, fsFilename);
		if (program && reloadedProgram) {
			glDeleteProgram(*program);
			*program = reloadedProgram;
      std::cout << "SHADER PROGRAM RELOADED WITH SUCCESS!\n" <<std::endl;
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Shader reload failed!\n");
		}
	}
}