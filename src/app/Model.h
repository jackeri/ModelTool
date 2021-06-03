#pragma once

#include "mt.h"

namespace mt {
	class Renderer;

	struct Point {
		glm::vec3 location{};
		glm::quat rotation{};
	};

	struct Vertex {
		glm::vec3 pos{};
		glm::vec3 normal{};
		glm::vec2 tex{};
	};

	struct Bounds {
		glm::vec3 min{};
		glm::vec3 max{};
	};

	namespace model {
		struct Hitbox;
	}

	void setupBounds(const Ref<mt::Renderer> &renderer, const Bounds &bounds, bool drawPoints = false);

	const int FRAME_BIND_POSE = -1;

	class Model {
	public:
		Model() = default;

		virtual ~Model() = default;

		virtual bool hasDrawable()
		{
			return (hasBindPose() || hasFrames());
		}

		virtual bool hasBindPose()
		{
			return false;
		}

		virtual bool hasFrames()
		{
			return (numFrames() > 0);
		}

		virtual int numFrames() = 0;

		virtual bool morphModel()
		{
			return false;
		}

		virtual bool skeletalModel()
		{
			return false;
		}

		virtual std::vector<mt::model::Hitbox> *getHitboxes()
		{
			return nullptr;
		}

		virtual void renderModel(Ref<mt::Renderer>) = 0;

		Point origin{};
		Point transform{};
		float scale{};
		std::string name;
		int currentFrame = FRAME_BIND_POSE;
	};

}
