#pragma once

#include "files.h"
#include "model.h"

#include <unordered_set>

namespace mt::model {

	/*! Shape of a hitbox */
	enum class HitboxShape
	{
		Box,
		Sphere,
		Cylinder,
		Capsule
	};

	/**
	 * @return name of a shape as written in the files ("box", "sphere", "cylinder", "capsule")
	 */
	const char *hitboxShapeName(HitboxShape shape);

	/**
	 * Shape from its file name
	 * @param name shape name (case insensitive)
	 * @param shape parsed shape
	 * @return false for an unknown name
	 */
	bool parseHitboxShape(const std::string &name, HitboxShape &shape);

	/*! Hitbox: a box, sphere, cylinder or capsule at a location and rotation, usually on a bone */
	struct Hitbox : public Bounds, public Point {
		std::string name{};							 ///< Name of this hitbox
		int parentJoint = -1;						 ///< Parent joint or tag which to link
		std::unordered_set<std::string> modifiers{}; ///< List of modifiers that this hitbox has
		HitboxShape shape = HitboxShape::Box;		 ///< shape, boxes use min / max, the others radius / height
		float radius = 0;							 ///< sphere, cylinder and capsule radius
		float height = 0;							 ///< cylinder and capsule height along the local Z axis, capsule end caps included

		/**
		 * Box hitbox
		 * @param size edge length of the box (also the diameter and height when the shape is changed)
		 */
		explicit Hitbox(float size = 0);

		/**
		 * Change the shape, keeping roughly the same size
		 * @param newShape shape to change to
		 */
		void setShape(HitboxShape newShape);

		/**
		 * Keep the sizes valid: no negative sizes, a capsule is at least as high as it is wide
		 */
		void clampSize();

		/**
		 * @return bounds of the shape in the hitbox's own space (before location and rotation)
		 */
		[[nodiscard]] Bounds localBounds() const;

		/**
		 * Wireframe of the shape in the hitbox's own space
		 * @param segments segments in a full circle
		 * @return line segments
		 */
		[[nodiscard]] std::vector<std::pair<glm::vec3, glm::vec3>> lines(int segments = 32) const;

		/**
		 * Load a list of hitboxes from a file
		 * @param file file to load from
		 * @param list vector to load the hitbox definitions into
		 */
		static void loadHitboxes(const Ref<mt::io::MTFile> &file, std::vector<Hitbox> &list);

		/**
		 * Persist a list of hitbox definitions to a file
		 * @param path local path to save the file to
		 * @param list vector of hitbox definitions
		 */
		static void saveHitboxes(const std::string &path, std::vector<Hitbox> &list);
	};
}
