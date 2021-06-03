#pragma once

#include "mt.h"
#include "Model.h"


namespace mt {
	class Camera;

	/*! Buffer which is used by the models and other to buffer their vertices and other data for drawing */
	struct RenderBuffer {
		glm::ivec3 indexes[60000]{};
		glm::fvec3 xyz[10000]{};
		glm::fvec3 normals[10000]{};
		glm::fvec2 texCoords[10000]{};

		bool triangles = true;
		float lineWidth = 1;

		glm::fvec4 color{0, 0, 0, 1};

		int numIndexes = 0;
		int numVertices = 0;
		int numNormals = 0;
		int numTexCoords = 0;

		/**
		 * Add index for drawing
		 * @param index to be added
		 */
		void addIndex(const glm::ivec3 &index)
		{
			indexes[numIndexes++] = index;
		}

		/**
		 * Get a index from the buffer to be set
		 * @return index value
		 */
		glm::ivec3 &getIndex()
		{
			return indexes[numIndexes++];
		}

		/**
		 * Add vertex for drawing
		 * @param vert to be added
		 */
		void addXyz(const glm::fvec3 &vert)
		{
			xyz[numVertices++] = vert;
		}

		/**
		 * Get a vertex from the buffer to be set
		 * @return vertex value
		 */
		glm::fvec3 &getXyz()
		{
			return xyz[numVertices++];
		}

		/**
		 * Add normal for drawing
		 * @param normal to be added
		 */
		void addNormal(const glm::fvec3 &normal)
		{
			normals[numNormals++] = normal;
		}

		/**
		 * Get a normal from the buffer to be set
		 * @return normal value
		 */
		glm::fvec3 &getNormal()
		{
			return normals[numNormals++];
		}

		/**
		 * Add texture coordinate for drawing
		 * @param texture coordinate to be added
		 */
		void addTexCoord(const glm::fvec2 &tex)
		{
			texCoords[numTexCoords++] = tex;
		}

		/**
		 * Get a texture coordinate from the buffer to be set
		 * @return texture coordinate value
		 */
		glm::fvec2 &getTexCoord()
		{
			return texCoords[numTexCoords++];
		}

		/**
		 * Clear this buffer (reset the counters)
		 * @param drawTriangles is the next drawing of triangles or lines
		 */
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

	/*! Renderer baseclass */
	class Renderer {
	public:
		Renderer() = default;

		virtual ~Renderer() = default;

		Renderer(const Renderer &other) = delete;

		Renderer(Renderer &other) = delete;

		Renderer(Renderer &&other) = delete;

		/**
		 * Setup mandatory fields, values and context data
		 */
		virtual void setup()
		{}

		/**
		 * Set the GL view size
		 * @param width width the view
		 * @param height height of the view
		 */
		virtual void setView(int width, int height) = 0;

		RenderBuffer &getBuffer()
		{
			return buffer;
		}

		/**
		 * Push a model view matrix the the stack
		 * @param matrix mdv matrix to push
		 */
		virtual void push(const glm::mat4x4 &matrix) = 0;

		/**
		 * Pop a matrix from the stack
		 */
		virtual void pop() = 0;

		/**
		 * Start a new frame and position the viewport with a camera
		 */
		virtual void startFrame(Camera &) = 0;

		/**
		 * Draw a grid
		 */
		virtual void grid() = 0;

		/**
		 * Render the current buffers data
		 */
		virtual void renderBuffer() = 0;

		/**
		 * End of the frame, finish up drawing
		 */
		virtual void endFrame() = 0;

		Renderer &operator=(const Renderer &other) = delete;

		Renderer &operator=(Renderer &other) = delete;

		Renderer &operator=(Renderer &&other) = delete;

	protected:
		RenderBuffer buffer{}; ///< Buffer that holds the drawing data
	};
}
