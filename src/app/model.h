#pragma once

#include "mt.h"

namespace mt {
	class Renderer;

	/*! Point in 3d space with rotation */
	struct Point {
		glm::vec3 location{}; ///< position vector
		glm::quat rotation{}; ///< quaternion rotation
	};

	/*! Model vertex with position, normal and texture information */
	struct Vertex {
		glm::vec3 pos{}; ///< position vector
		glm::vec3 normal{}; ///< normal vector
		glm::vec2 tex{}; ///< texture coordinates
	};

	/*! Model bounds data */
	struct Bounds {
		glm::vec3 min{}; ///< minimal point (corner of the quad)
		glm::vec3 max{}; ///< maximal point (corner of the quad)
	};

	// forward declaration
	namespace model {
		struct Hitbox;
	}

	/**
	 * Setup the render buffer from the bounds data
	 * @param renderer renderer which is used
	 * @param bounds bounds which should be drawn
	 * @param drawPoints should the corners be highlighted
	 */
	void setupBounds(const Ref<mt::Renderer> &renderer, const Bounds &bounds, bool drawPoints = false);

	const int FRAME_BIND_POSE = -1; ///< "Number" of the bind pose frame in the model frames array, which go from 0 -> *

	class Model {
	public:
		Model() = default;

		virtual ~Model() = default;

		/**
		 * Does this model have something to draw
		 * @return true if this model can be drawn
		 */
		virtual bool hasDrawable()
		{
			return (hasBindPose() || hasFrames());
		}

		/**
		 * Does this model have bind pose information
		 * @return true if bind pose information exists
		 */
		virtual bool hasBindPose()
		{
			return false;
		}

		/**
		 * Does this model have frames
		 * @return true if there are 1 or more frames
		 */
		virtual bool hasFrames()
		{
			return (numFrames() > 0);
		}

		/**
		 * Check how many frames the model has available
		 * @return number of frames
		 */
		virtual int numFrames() = 0;

		/**
		 * Is this model a morph animated model (no skeleton)
		 * @return true if the model is a morph model
		 */
		virtual bool morphModel()
		{
			return false;
		}

		/**
		 * Is this model a Skeletal animated model (the animation is not baked in)
		 * @return true if the model is a skeletal model
		 */
		virtual bool skeletalModel()
		{
			return false;
		}

		/**
		 * If the model has hitboxes then return the vector
		 * @return vector of hitboxes if available
		 */
		virtual std::vector<mt::model::Hitbox> *getHitboxes()
		{
			return nullptr;
		}

		/**
		 * Does the model have tags
		 * @return true if tags are present
		 */
		virtual bool hasTags()
		{
			return false;
		}

		/**
		 * Render this model with a renderer
		 * @param renderer renderer to be used
		 */
		virtual void renderModel(Ref<mt::Renderer> renderer) = 0;

		Point origin{}; ///< Origin point of this model
		Point transform{}; ///< Transforms applied to this model
		float scale{}; ///< Scale of this model
		std::string name; ///< Name of this model
		int currentFrame = FRAME_BIND_POSE; ///< Currently used frame
	};

}
