#pragma once

#include "mt.h"

namespace mt {
	struct Point {
		glm::vec3 location{};
		glm::quat rotation{};
	};

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 tex;
	};

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

		bool hasTransform() const
		{
			return false;
		}

		virtual bool morphModel()
		{
			return false;
		}

		virtual bool skeletalModel()
		{
			return false;
		}

		Point origin{};
		Point transform{};
		float scale{};
		std::string name;
	};

}
