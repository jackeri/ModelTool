#pragma once

#include "mt.h"

namespace mt {

	/*! Opengl GLSL shader application wrapper */
	class GLShader {
	public:
		GLShader();

		~GLShader();

		/**
		 * Construct an glsl application
		 * @param vertex_code vertex program code
		 * @param fragment_code fragment program code
		 */
		void init(const std::string &vertex_code, const std::string &fragment_code);

		/**
		 * Check if the program was compiled OK and is valid
		 * @return if the program valid
		 */
		bool valid() const;

		/**
		 * Bind the application
		 */
		void use() const;

		/**
		 * Get the uniform location offset from the application
		 * @param name uniform name
		 * @return offset value
		 */
		GLint getUniformLocation(const std::string &name) const;

		/**
		 * Get the attribute location offset from the application
		 * @param name attribute name
		 * @return offset value
		 */
		GLint getAttribLocation(const std::string &name) const;

		/**
		 * Set uniform value
		 * @tparam T value type
		 * @param name uniform name
		 * @param val value to be set
		 */
		template<typename T>
		void setUniform(const std::string &name, T val);

		/**
		 * Set uniform value
		 * @tparam T value type
		 * @param name uniform name
		 * @param val1 value to be set
		 * @param val2 value to be set
		 */
		template<typename T>
		void setUniform(const std::string &name, T val1, T val2);


		/**
		 * Set uniform value
		 * @tparam T value type
		 * @param name uniform name
		 * @param val1 value to be set
		 * @param val2 value to be set
		 * @param val3 value to be set
		 */
		template<typename T>
		void setUniform(const std::string &name, T val1, T val2, T val3);

	private:
		/**
		 * Check for shader compilation errors
		 */
		void checkCompileErr() const;

		/**
		 * Check for application linking errors
		 */
		void checkLinkingErr() const;

		/**
		 * Compile the shaders
		 */
		void compile();

		/**
		 * link the application
		 */
		void link();

		GLint vertId{}, fragId{}, progId{}; ///< Shaders and program ids
		std::string vertexCode; ///< Vertex shader code
		std::string fragmentCode; ///< Fragment shader code
		bool programValid{false}; ///< was the compile and linking process OK
	};
}
