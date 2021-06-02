#pragma once

#include "mt.h"
#include "Model.h"


namespace mt {
	class Camera;

	struct RenderBuffer {
		glm::ivec3 indexes[60000]{};
		glm::fvec3 xyz[10000]{};
		glm::fvec3 normals[10000]{};
		glm::fvec2 texCoords[10000]{};

		bool triangles = true;

		glm::fvec4 color{0, 0, 0, 1};

		int numIndexes = 0;
		int numVertices = 0;
		int numNormals = 0;
		int numTexCoords = 0;

		void addIndex(const glm::ivec3 &index)
		{
			indexes[numIndexes++] = index;
		}

		glm::ivec3 &getIndex()
		{
			return indexes[numIndexes++];
		}

		void addXyz(const glm::fvec3 &vert)
		{
			xyz[numVertices++] = vert;
		}

		glm::fvec3 &getXyz()
		{
			return xyz[numVertices++];
		}

		void addNormal(const glm::fvec3 &normal)
		{
			normals[numNormals++] = normal;
		}

		glm::fvec3 &getNormal()
		{
			return normals[numNormals++];
		}

		void addTexCoord(const glm::fvec2 &tex)
		{
			texCoords[numTexCoords++] = tex;
		}

		glm::fvec2 &getTexCoord()
		{
			return texCoords[numTexCoords++];
		}

		void clear(bool drawTriangles = true)
		{
			numIndexes = 0;
			numVertices = 0;
			numNormals = 0;
			numTexCoords = 0;

			color = {0, 0, 0, 1};

			triangles = drawTriangles;
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

		virtual void push(const glm::mat4x4 &matrix) = 0;

		virtual void pop() = 0;

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
