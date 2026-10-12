#include "model_scripts.h"
#include "script_stream.h"

namespace mt::model {
	using namespace mt::model::skeletal;

	static int parseInt(ScriptStream &stream, const std::string &field)
	{
		std::string token = stream.token(false);
		try
		{
			size_t used = 0;
			int value = std::stoi(token, &used);
			if (used == token.size())
			{
				return value;
			}
		}
		catch (const std::exception &)
		{}

		throw mt_ex("Invalid " + field + ": '" + token + "' (line " + std::to_string(stream.line() + 1) + ")");
	}

	void loadAninc(Model *model, const Ref<mt::io::MTFile> &file)
	{
		if (!model)
		{
			throw mt_ex("No model to load the animation sequences into");
		}

		ScriptStream stream(file);
		std::vector<Animation> animations;

		while (stream.hasNext())
		{
			Animation animation;
			animation.name = stream.token();
			animation.startFrame = parseInt(stream, "first frame");
			animation.numFrames = parseInt(stream, "length");
			animation.loop = parseInt(stream, "looping") != 0;
			animation.fps = parseInt(stream, "fps");
			animation.moveSpeed = parseInt(stream, "move speed");
			animation.transition = parseInt(stream, "transition");
			animation.reversed = parseInt(stream, "reversed") != 0;

			if (animation.numFrames < 0 || animation.startFrame < 0)
			{
				throw mt_ex("Invalid frame range for animation: " + animation.name);
			}

			animations.push_back(animation);
		}

		model->animations = std::move(animations);
	}

	void loadTags(SkeletalModel &model, const Ref<mt::io::MTFile> &file)
	{
		ScriptStream stream(file);
		std::vector<BoneTag> tags;

		while (stream.hasNext())
		{
			if (stream.token() != "tags" || stream.token() != "{")
			{
				throw mt_ex("Invalid tags file, expected 'tags {' but found: " + stream.last());
			}

			while (stream.hasNext() && stream.peekNext() != "}")
			{
				BoneTag tag;
				tag.name = stream.token();
				std::string parentName = stream.token(false);

				auto parent = std::find_if(model.joints.begin(), model.joints.end(), [&](const Joint &joint) {
					return joint.name.size() == parentName.size()
						   && std::equal(joint.name.begin(), joint.name.end(), parentName.begin(),
										 [](char a, char b) { return std::tolower(a) == std::tolower(b); });
				});

				if (parent == model.joints.end())
				{
					throw mt_ex("Parent bone '" + parentName + "' not found for tag: " + tag.name);
				}

				tag.parentBone = int(parent - model.joints.begin());
				stream.parse1DMatrix(3, glm::value_ptr(tag.location));
				stream.parse1DMatrix(4, glm::value_ptr(tag.rotation));
				tag.rotation = glm::normalize(tag.rotation);
				tags.push_back(tag);
			}

			if (stream.token() != "}")
			{
				throw mt_ex("Invalid tags file, missing '}'");
			}
		}

		model.tags.insert(model.tags.end(), tags.begin(), tags.end());
	}
}
