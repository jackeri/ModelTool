#pragma once

#include "library.h"
#include "mt_path.h"

namespace mt {
	class Renderer;

	/*! Point in 3d space with rotation */
	struct Point {
		glm::vec3 location{};						///< position vector
		glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; ///< quaternion rotation

		/**
		 * Build a point from an idTech3 style origin and axis (axis[0] forward, axis[1] left, axis[2] up)
		 * @param origin location
		 * @param axis three axis vectors
		 * @return point with the rotation matching the axis
		 */
		static Point fromAxis(const glm::vec3 &origin, const glm::mat3 &axis)
		{
			return {origin, glm::normalize(glm::quat_cast(axis))};
		}

		/**
		 * @return the rotation as an idTech3 style axis, axis[i] is the i:th axis vector
		 */
		[[nodiscard]] glm::mat3 axis() const
		{
			return glm::mat3_cast(rotation);
		}
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

		/*! Named animation sequence, a range of frames in the model (like the entries of an .aninc file) */
		struct Animation {
			std::string name; ///< Name of the animation
			int startFrame{}; ///< First frame of the animation
			int numFrames{};  ///< Number of frames in the animation
			int fps{};		  ///< Frames per second
			bool loop{};	  ///< Should the animation loop
			bool reversed{};  ///< Is the animation played backwards (from startFrame down)
			int moveSpeed{};  ///< Movement speed of the animation (game side value)
			int transition{}; ///< Transition time to this animation in milliseconds (game side value)

			/**
			 * @return the last frame played, taking the playback direction into account
			 */
			[[nodiscard]] int lastFrame() const
			{
				return reversed ? startFrame - numFrames + 1 : startFrame + numFrames - 1;
			}
		};

		namespace io {
			/*! Loads a model from a file, throws on invalid data */
			using ModelLoader = std::function<Model *(const Ref<mt::io::MTFile> &)>;
			/*! Loads animation data from a file into an existing model, throws on invalid data */
			using AnimationLoader = std::function<void(Model *, const Ref<mt::io::MTFile> &)>;
			/*! Writes a model to a local path (formats with several files derive the other names from it) */
			using ModelWriter = std::function<void(const Model &, const std::string &)>;
			/*! Writes a models animation data to a local path */
			using AnimationWriter = std::function<void(const Model &, const std::string &)>;

			/**
			 * Normalize a file extension or a file name to a lowercase extension with the leading dot ("MD3" -> ".md3")
			 * @param extOrName extension with or without the dot, or a file name/path
			 * @return normalized extension, empty if there is none
			 */
			std::string normalizeExtension(const std::string &extOrName);

			/**
			 * Load a model using the loader registered for the file's extension
			 * @param file file to load
			 * @return loaded model, owned by the caller
			 * @throws mt::mt_exception if the file is missing, the extension is unknown or the data is invalid
			 */
			Model *loadModel(const Ref<mt::io::MTFile> &file);

			/**
			 * Load animation data into a model using the loader registered for the file's extension
			 * @param parent model to load the animation into
			 * @param file file to load
			 * @throws mt::mt_exception if the file is missing, the extension is unknown or the data is invalid
			 */
			void loadAnimation(Model *parent, const Ref<mt::io::MTFile> &file);

			/**
			 * Write a model using the writer registered for the path's extension
			 * @param model model to write
			 * @param path local path, the extension selects the format
			 */
			void writeModel(const Model &model, const std::string &path);

			/**
			 * Write a models animation data using the writer registered for the path's extension
			 * @param model model to write
			 * @param path local path, the extension selects the format
			 */
			void writeAnimation(const Model &model, const std::string &path);

			void registerModelLoader(const std::string &ext, ModelLoader loader);
			void registerAnimationLoader(const std::string &ext, AnimationLoader loader);
			void registerModelWriter(const std::string &ext, ModelWriter writer);
			void registerAnimationWriter(const std::string &ext, AnimationWriter writer);

			/** @return sorted extensions that have a model loader */
			std::vector<std::string> modelExtensions();
			/** @return sorted extensions that have an animation loader */
			std::vector<std::string> animationExtensions();
			/** @return sorted extensions that have a model writer */
			std::vector<std::string> modelWriterExtensions();
			/** @return sorted extensions that have an animation writer */
			std::vector<std::string> animationWriterExtensions();
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

		Point origin{};								///< Origin point of this model
		Point transform{};							///< Transforms applied to this model
		float scale = 1.0f;							///< Scale of this model
		std::string name;							///< Name of this model
		std::vector<model::Animation> animations{}; ///< Animation sequences defined for this model
		int currentFrame = FRAME_BIND_POSE;			///< Currently used frame
	};

}
