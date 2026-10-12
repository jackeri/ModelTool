#pragma once

#include "mt.h"
#include "camera.h"
#include "renderer.h"
#include "framebuffer.h"
#include "posed_model.h"

namespace mt {

	/*! Scene panel which hold the actual 3d view */
	class ScenePanel {
	  public:
		ScenePanel();

		~ScenePanel();

		/**
		 * Render the scene panel and handle input
		 */
		void render();

		/**
		 * Render the scene into the panel's framebuffer without the UI
		 * @param width width in pixels
		 * @param height height in pixels
		 */
		void renderScene(int width, int height);

		/**
		 * Save the last rendered scene as a PNG
		 * @param path output path
		 * @return true if the image was written
		 */
		bool saveImage(const std::string &path);

		/**
		 * Write RGBA pixels (OpenGL row order, bottom up) as a PNG
		 */
		static bool writePng(const std::string &path, int width, int height, const std::vector<byte> &pixels);

		/**
		 * @return camera of the scene
		 */
		Camera &getCamera()
		{
			return camera;
		}

	  private:
		/**
		 * Handle the mouse in the scene (rotate, pan and zoom)
		 */
		void handleInput();

		/**
		 * Draw the tag names on top of the scene image
		 * @param origin screen position of the scene image
		 */
		void drawLabels(const ImVec2 &origin);

		glm::ivec2 size{};								   ///< Framebuffer size in pixels
		glm::vec2 viewSize{};							   ///< Size of the scene image in the window (smaller than size on HiDPI screens)
		Camera camera;									   ///< Camera instance which is used in the scene panel
		Ref<Framebuffer> fbo;							   ///< Frame buffer for 3d view drawing
		Ref<Renderer> renderer;							   ///< Renderer which is used for the 3d view
		model::PosedModel posed{};						   ///< model pose of the last frame
		glm::mat4 modelMatrix{1.0f};					   ///< model transform of the last frame
		std::vector<model::PosedModel> posedAttachments{}; ///< attached models of the last frame
		std::vector<glm::mat4> attachmentMatrices{};	   ///< scene matrices of the attached models
		bool sceneHovered = false;						   ///< state flag for mouse hover scene panel
	};
}
