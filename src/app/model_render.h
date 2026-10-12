#pragma once

#include "mt.h"
#include "renderer.h"
#include "posed_model.h"
#include "playback.h"
#include "texture_cache.h"

namespace mt {
	struct ViewSettings;

	/**
	 * Pose to draw for the model's current frame and the playback state
	 * @param model model to draw
	 * @param view view settings (level of detail)
	 * @param playback playback clock, blends between frames while playing
	 * @return pose options
	 */
	model::PoseOptions poseOptions(Model &model, const ViewSettings &view, const model::Playback &playback);

	/**
	 * Matrix of the model's transform (location, rotation and scale)
	 */
	glm::mat4 modelTransform(const Model &model);

	/**
	 * Draw a posed model with the overlays enabled in the view settings
	 * @param renderer renderer to draw with
	 * @param posed posed model
	 * @param view what to draw
	 * @param textures texture cache for the materials
	 * @param source file source for the textures
	 * @param modelPath path of the model (textures are looked up next to it)
	 * @param size size of the model (bounds diagonal), the overlays are scaled to it
	 */
	void drawModel(Renderer &renderer, const model::PosedModel &posed, const ViewSettings &view, TextureCache &textures,
				   io::FileSource &source, const std::string &modelPath, float size);

	/**
	 * Bounds of the posed meshes and joints
	 * @param posed posed model
	 * @param out bounds
	 * @return false if there is nothing to measure
	 */
	bool posedBounds(const model::PosedModel &posed, Bounds &out);
}
