#pragma once

#include "library.h"
#include "mt_path.h"

namespace mt {
	class Renderer;

	/*! Point in 3d space with rotation */
	struct Point {
		glm::vec3 location{}; ///< position vector
		glm::quat rotation{}; ///< quaternion rotation
	};

	/*! Model vertex with position, normal and texture information */
	struct Vertex {
		glm::vec3 pos{};	///< position vector
		glm::vec3 normal{}; ///< normal vector
		glm::vec2 tex{};	///< texture coordinates
	};

	/*! Model bounds data */
	struct Bounds {
		glm::vec3 min{}; ///< minimal point (corner of the quad)
		glm::vec3 max{}; ///< maximal point (corner of the quad)
	};

	// Model forward declaration
	struct Model;

	// forward declaration
	namespace model {
		struct Hitbox;

		struct Animation {
			std::string name; ///< Name of the animation
			int startFrame{}; ///< Start frame of the animation
			int endFrame{};	  ///< End frame of the animation
			int fps{};		  ///< Frames per second
			bool loop{};	  ///< Should the animation loop
		};

		namespace io {
			Model *loadModel(const Ref<mt::io::MTFile> &file);
			void loadAnimation(Model *parent, const Ref<mt::io::MTFile> &file);
		}
	}

	const int FRAME_BIND_POSE = -1; ///< "Number" of the bind pose frame in the model frames array, which go from 0 -> *

	struct Model {
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

		Point origin{};						///< Origin point of this model
		Point transform{};					///< Transforms applied to this model
		float scale{};						///< Scale of this model
		std::string name;					///< Name of this model
		int currentFrame = FRAME_BIND_POSE; ///< Currently used frame
	};

}
