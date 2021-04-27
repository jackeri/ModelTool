#pragma once

#include "mt.h"

namespace mt {
	class Texture {
	public:
		explicit Texture(std::string name);

		~Texture();

		bool load(const byte_buffer& data);

		void bind() const;

		void unbind();

		glm::vec2 size();

		GLuint id();

	private:
		int width = -1, height = -1;
		std::string name{};
		GLuint textId = 0;
	};
}
