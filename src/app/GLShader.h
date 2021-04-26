#pragma once

#include <string>
#include <vector>

#include <GL/glew.h>

namespace mt {
	class GLShader {
	public:
		GLShader();

		~GLShader();

		void init(const std::string &vertex_code, const std::string &fragment_code);

		bool valid() const;

		void use() const;

		GLint getUniformLocation(const std::string &name) const;

		GLint getAttribLocation(const std::string &name) const;

		template<typename T>
		void setUniform(const std::string &name, T val);

		template<typename T>
		void setUniform(const std::string &name, T val1, T val2);

		template<typename T>
		void setUniform(const std::string &name, T val1, T val2, T val3);

	private:
		void checkCompileErr() const;

		void checkLinkingErr() const;

		void compile();

		void link();

		GLint vertId{}, fragId{}, progId{};
		std::string vertexCode;
		std::string fragmentCode;
		bool programValid{false};
	};
}
