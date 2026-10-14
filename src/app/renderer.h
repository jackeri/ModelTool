#pragma once

#include "mt.h"
#include "model.h"

#include <array>
#include "../lib/shader_script.h"

namespace mt {
	class Camera;
	class Texture;

	/*! Vertex of a line or a point */
	struct LineVertex {
		glm::vec3 pos{};
		glm::vec4 color{1, 1, 1, 1};
	};

	/*! How a mesh is drawn */
	struct MeshStyle {
		glm::vec4 color{1, 1, 1, 1};							///< color multiplied with the texture
		const Texture *texture = nullptr;						///< texture, nullptr for no texture
		bool wireframe = false;									///< draw the triangle edges only
		bool lighting = true;									///< shade with the normals
		bool alpha = false;										///< blend with the texture alpha
		model::ShaderCull cull = model::ShaderCull::None;		///< sides that are drawn (idTech front faces are clockwise)
		model::AlphaFunc alphaFunc = model::AlphaFunc::None;	///< drop pixels by their alpha, like the shader's alphaFunc
		bool blend = false;										///< blend with blendSrc / blendDst (overrides alpha)
		model::BlendFactor blendSrc = model::BlendFactor::One;	///< blend source factor
		model::BlendFactor blendDst = model::BlendFactor::Zero; ///< blend destination factor
		bool depthWrite = true;
		bool flat = false;		///< light each triangle with its own normal (flat shading)
		float gamma = 1.0f;		///< like the game's r_gamma, 1 = unchanged
		float intensity = 1.0f; ///< like the game's r_intensity: texture colors are multiplied, 1 = unchanged
								///< ///< write the depth
	};

	/*! Mesh data to draw, the vectors are not copied */
	struct MeshData {
		const std::vector<glm::vec3> &positions;
		const std::vector<glm::vec3> &normals;
		const std::vector<glm::vec2> &texCoords;
		const std::vector<glm::ivec3> &tris;
	};

	/*! Renderer base class */
	class Renderer {
	  public:
		Renderer() = default;

		virtual ~Renderer() = default;

		Renderer(const Renderer &other) = delete;

		Renderer &operator=(const Renderer &other) = delete;

		/**
		 * Start a new frame
		 * @param camera camera to view the scene with
		 * @param width width of the view in pixels
		 * @param height height of the view in pixels
		 * @param clearColor background color
		 */
		virtual void startFrame(const Camera &camera, int width, int height, const glm::vec4 &clearColor) = 0;

		/**
		 * End of the frame, finish up drawing
		 */
		virtual void endFrame() = 0;

		/**
		 * Draw a mesh with the current model matrix
		 */
		virtual void drawMesh(const MeshData &mesh, const MeshStyle &style) = 0;

		/**
		 * Draw lines (every two vertices are a line) with the current model matrix
		 * @param vertices line end points
		 * @param depthTest hide the lines behind the meshes
		 */
		virtual void drawLines(const std::vector<LineVertex> &vertices, bool depthTest = false) = 0;

		/**
		 * Draw points with the current model matrix
		 * @param vertices points
		 * @param size point size in pixels
		 * @param depthTest hide the points behind the meshes
		 */
		virtual void drawPoints(const std::vector<LineVertex> &vertices, float size, bool depthTest = false) = 0;

		/**
		 * Draw the ground grid with the X and Y axis to the edges of the view, fading out with the distance, and the Z
		 * axis line
		 * @param size size of the model, the grid steps are powers of two (about 40 steps over the model)
		 * @param color grid line color
		 */
		virtual void grid(float size, const glm::vec4 &color) = 0;

		/**
		 * Grid line color that shows on a background: dark lines on light backgrounds, light lines on dark ones
		 * @param background background color
		 * @return grid color
		 */
		static glm::vec4 gridColor(const glm::vec4 &background);

		/**
		 * Fill the view with a vertical gradient, before anything else is drawn
		 * @param top color at the top of the view
		 * @param bottom color at the bottom
		 */
		virtual void background(const glm::vec4 &top, const glm::vec4 &bottom) = 0;

		/**
		 * Draw a sky box around the camera like the game (the sides in the game's order rt, bk, lf, ft, up, dn), before
		 * anything else is drawn
		 * @param sides textures of the sides, nullptr draws a white side
		 * @param gamma like the game's r_gamma
		 * @param intensity like the game's r_intensity
		 */
		virtual void skyBox(const std::array<const Texture *, 6> &sides, float gamma, float intensity) = 0;

		/**
		 * Draw the edges of bounds with the current model matrix
		 * @param bounds bounds to draw
		 * @param color line color
		 */
		void bounds(const Bounds &bounds, const glm::vec4 &color);

		/**
		 * Draw the axis of a tag (red x, green y, blue z) at the origin of the current model matrix
		 * @param size length of the axis lines
		 */
		void axis(float size);

		/**
		 * Push a matrix to the model matrix stack (multiplied with the current matrix)
		 * @param matrix matrix to push
		 */
		void push(const glm::mat4 &matrix);

		/**
		 * Pop a matrix from the model matrix stack
		 */
		void pop();

		/**
		 * @return current model matrix
		 */
		[[nodiscard]] const glm::mat4 &modelMatrix() const
		{
			return stack.back();
		}

		/**
		 * @return projection * view matrix of the current frame
		 */
		[[nodiscard]] const glm::mat4 &viewProjection() const
		{
			return viewProj;
		}

		float pixelScale = 1.0f; ///< framebuffer pixels per window unit (2 on most HiDPI screens), scales point sizes

	  protected:
		std::vector<glm::mat4> stack{glm::mat4{1.0f}}; ///< model matrix stack
		glm::mat4 viewProj{1.0f};					   ///< projection * view of the frame
	};
}
