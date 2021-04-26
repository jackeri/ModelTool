#pragma once

#include <string>
#include <vector>

namespace mt {
	class GLShader {
	public:
		GLShader();

		~GLShader();

		void init(const std::string &vertex_code, const std::string &fragment_code);

		bool valid();

		void use() const;

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

		unsigned int vertId{}, fragId{}, progId{};
		std::string vertexCode;
		std::string fragmentCode;
		bool programValid{false};
	};
}
