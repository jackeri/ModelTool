#include "GLShader.h"

#include <sstream>
#include <iostream>

mt::GLShader::GLShader() = default;

mt::GLShader::~GLShader()
{
	if (progId)
	{
		glDeleteProgram(progId);
		progId = 0;
	}
}

void mt::GLShader::init(const std::string &vertex_code, const std::string &fragment_code)
{
	vertexCode = vertex_code;
	fragmentCode = fragment_code;
	compile();
	link();
}

void mt::GLShader::compile()
{
	const char *vertex = vertexCode.c_str();
	vertId = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertId, 1, &vertex, nullptr);
	glCompileShader(vertId);

	const char *fragment = fragmentCode.c_str();
	fragId = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragId, 1, &fragment, nullptr);
	glCompileShader(fragId);

	checkCompileErr();
}

void mt::GLShader::link()
{
	progId = glCreateProgram();
	glAttachShader(progId, vertId);
	glAttachShader(progId, fragId);
	glLinkProgram(progId);

	checkLinkingErr();

	// We dont need the shaders anymore, as they are linked to the program directly
	glDeleteShader(vertId);
	glDeleteShader(fragId);

	vertId = 0;
	fragId = 0;
}

bool mt::GLShader::valid() const
{
	return programValid;
}

void mt::GLShader::use() const
{
	glUseProgram(progId);
}

GLint mt::GLShader::getUniformLocation(const std::string &name) const
{
	return glGetUniformLocation(progId, name.c_str());
}

GLint mt::GLShader::getAttribLocation(const std::string &name) const
{
	return glGetAttribLocation(progId, name.c_str());
}

template<>
void mt::GLShader::setUniform<int>(const std::string &name, int val)
{
	glUniform1i(getUniformLocation(name), val);
}

template<>
void mt::GLShader::setUniform<bool>(const std::string &name, bool val)
{
	glUniform1i(getUniformLocation(name), val);
}

template<>
void mt::GLShader::setUniform<float>(const std::string &name, float val)
{
	glUniform1f(getUniformLocation(name), val);
}

template<>
void mt::GLShader::setUniform<float>(const std::string &name, float val1, float val2)
{
	glUniform2f(getUniformLocation(name), val1, val2);
}

template<>
void mt::GLShader::setUniform<float>(const std::string &name, float val1, float val2, float val3)
{
	glUniform3f(getUniformLocation(name), val1, val2, val3);
}

template<>
void mt::GLShader::setUniform<float *>(const std::string &name, float *val)
{
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, val);
}

void mt::GLShader::checkCompileErr() const
{
	int success;
	char infoLog[1024];
	glGetShaderiv(vertId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertId, 1024, nullptr, infoLog);
		std::cout << "Error compiling Vertex Shader:\n" << infoLog << std::endl;
	}
	glGetShaderiv(fragId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragId, 1024, nullptr, infoLog);
		std::cout << "Error compiling Fragment Shader:\n" << infoLog << std::endl;
	}
}

void mt::GLShader::checkLinkingErr() const
{
	int success;
	char infoLog[1024];
	glGetProgramiv(progId, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(progId, 1024, nullptr, infoLog);
		std::cout << "Error Linking Shader Program:\n" << infoLog << std::endl;
	}
}
