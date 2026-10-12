#pragma once

#include "mt.h"
#include "renderer.h"
#include "gl_shader.h"

namespace mt {

	/*! OpenGL 4.1 core profile renderer */
	class GLRenderer : public Renderer {
	  public:
		GLRenderer();

		~GLRenderer() override;

		void startFrame(const Camera &camera, int width, int height, const glm::vec4 &clearColor) override;

		void endFrame() override;

		void drawMesh(const MeshData &mesh, const MeshStyle &style) override;

		void drawLines(const std::vector<LineVertex> &vertices, bool depthTest) override;

		void drawPoints(const std::vector<LineVertex> &vertices, float size, bool depthTest) override;

		void grid(float size, const glm::vec4 &color) override;

	  private:
		void drawLineVertices(const std::vector<LineVertex> &vertices, GLenum mode, float pointSize, bool depthTest);

		GLShader meshShader{};
		GLShader lineShader{};
		GLShader gridShader{};

		GLuint meshVao = 0, meshVbo = 0, meshIbo = 0;
		GLuint lineVao = 0, lineVbo = 0;
		GLuint gridVao = 0, gridVbo = 0;
		glm::vec3 cameraTarget{};	 ///< point the camera looks at (the grid fades out around it)
		float cameraDistance = 1.0f; ///< distance of the camera from its target
		float cameraFar = 1000.0f;	 ///< far clipping plane
		GLuint whiteTexture = 0;	 ///< bound when a mesh has no texture, so the sampler always has a valid texture

		std::vector<float> meshVertices{};
		std::vector<uint32_t> meshIndices{};
	};
}
