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

		/**
		 * Draw the orientation gizmo in the top right corner of the scene: the world axes as the camera sees them
		 * (idTech: +X forward, +Y left, +Z up). Clicking an axis looks from that side
		 * @param origin screen position of the scene image
		 */
		void drawGizmo(const ImVec2 &origin);

		/**
		 * Draw the move / rotate gizmo of the selected tag or hitbox and apply its changes to the model
		 * @param origin screen position of the scene image
		 */
		void drawEditGizmo(const ImVec2 &origin);

		/**
		 * Draw the gizmo toolbar (move / rotate, local / world) in the top left corner of the scene
		 * @param origin screen position of the scene image
		 * @param label what is edited
		 * @param morphTag a morph model tag is edited (it has the "all frames" option)
		 * @param canScale a hitbox is edited (it can be scaled)
		 */
		void drawEditToolbar(const ImVec2 &origin, const std::string &label, bool morphTag, bool canScale);

		glm::ivec2 size{};								   ///< Framebuffer size in pixels
		glm::vec2 viewSize{};							   ///< Size of the scene image in the window (smaller than size on HiDPI screens)
		Camera camera;									   ///< Camera instance which is used in the scene panel
		Ref<Framebuffer> fbo;							   ///< Frame buffer for 3d view drawing
		Ref<Renderer> renderer;							   ///< Renderer which is used for the 3d view
		model::PosedModel posed{};						   ///< model pose of the last frame
		glm::mat4 modelMatrix{1.0f};					   ///< model transform of the last frame
		std::vector<model::PosedModel> posedAttachments{}; ///< attached models of the last frame
		std::vector<glm::mat4> attachmentMatrices{};	   ///< scene matrices of the attached models
		model::Hitbox scaleStart{};						   ///< the edited hitbox when the scale drag started
		bool gizmoDrag = false;							   ///< the mouse button went down on the edit gizmo, the camera does not move
		bool sceneHovered = false;						   ///< state flag for mouse hover scene panel
	};
}
