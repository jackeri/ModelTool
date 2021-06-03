#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"

#include <unordered_set>

namespace mt::model {
	struct Hitbox : public Bounds, public Point {
		std::string name{};
		int parentJoint = -1;
		std::unordered_set<std::string> modifiers{};

		explicit Hitbox(float size = 0);

		static void loadHitboxes(const Ref<IO::MTFile> &file, std::vector<Hitbox> &list);

		static void saveHitboxes(const std::string &path, std::vector<Hitbox> &list);
	};
}
