#pragma once

#include "mt.h"

namespace mt {
	class Camera;

	struct RenderBuffer {
		glm::ivec3 indexes[60000]{};
		glm::fvec3 xyz[10000]{};
		glm::fvec3 normals[10000]{};
		glm::fvec2 texCoords[10000]{};

		int numIndexes = 0;
		int numVertices = 0;
		int numNormals = 0;
		int numTexCoords = 0;

		void addIndex(const glm::ivec3 &index)
		{
			indexes[numIndexes++] = index;
		}

		void addXyz(const glm::fvec3 &vert)
		{
			xyz[numVertices++] = glm::fvec4(vert, 0);
		}

		void addNormal(const glm::fvec3 &normal)
		{
			normals[numNormals++] = glm::fvec4(normal, 0);
		}

		void addTexCoord(const glm::fvec2 &tex)
		{
			texCoords[numTexCoords++] = tex;
		}

		void clear()
		{
			numIndexes = 0;
			numVertices = 0;
			numNormals = 0;
			numTexCoords = 0;
		}
	};

	class Renderer {
	public:
		Renderer() = default;

		virtual ~Renderer() = default;

		Renderer(const Renderer &other) = delete;

		Renderer(Renderer &other) = delete;

		Renderer(Renderer &&other) = delete;

		virtual void setup()
		{}

		virtual void setView(int width, int height) = 0;

		RenderBuffer &getBuffer()
		{
			return buffer;
		}

		virtual void startFrame(Camera &) = 0;

		virtual void grid() = 0;

		virtual void renderBuffer() = 0;

		virtual void endFrame() = 0;

		Renderer &operator=(const Renderer &other) = delete;

		Renderer &operator=(Renderer &other) = delete;

		Renderer &operator=(Renderer &&other) = delete;

	protected:
		RenderBuffer buffer{};
	};
}
