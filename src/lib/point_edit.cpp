#include "point_edit.h"

namespace mt::model {

	glm::mat4 pointMatrix(const Point &point)
	{
		return glm::translate(point.location) * glm::toMat4(point.rotation);
	}

	Point pointFromMatrix(const glm::mat4 &matrix)
	{
		glm::mat3 axes(matrix);
		for (int i = 0; i < 3; i++)
		{
			const float length = glm::length(axes[i]);
			if (length > 0.0f)
			{
				axes[i] /= length;
			}
		}
		Point point;
		point.location = glm::vec3(matrix[3]);
		point.rotation = glm::normalize(glm::quat_cast(axes));
		return point;
	}

	Point localPoint(const glm::mat4 &parentWorld, const glm::mat4 &world)
	{
		return pointFromMatrix(glm::inverse(parentWorld) * world);
	}

	void moveMorphTag(morph::MorphModel &model, int tag, int frame, const Point &point, bool allFrames)
	{
		if (tag < 0 || tag >= int(model.tags.size()))
		{
			return;
		}
		auto &frames = model.tags[size_t(tag)].frames;
		if (frame < 0 || frame >= int(frames.size()))
		{
			return;
		}

		if (!allFrames)
		{
			frames[size_t(frame)] = point;
			return;
		}

		// The change of the edited frame, applied to every frame
		const glm::mat4 change = pointMatrix(point) * glm::inverse(pointMatrix(frames[size_t(frame)]));
		for (auto &framePoint : frames)
		{
			framePoint = pointFromMatrix(change * pointMatrix(framePoint));
		}
		frames[size_t(frame)] = point;
	}
}
