#include "model_scripts.h"
#include "script_stream.h"
#include "model_export.h"
#include "binary_writer.h"

#include <iomanip>

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

	std::vector<Animation> parseAnincLines(ScriptStream &stream)
	{
		std::vector<Animation> animations;

		while (stream.hasNext() && stream.peekNext() != "}")
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

		return animations;
	}

	void loadAninc(Model *model, const Ref<mt::io::MTFile> &file)
	{
		if (!model)
		{
			throw mt_ex("No model to load the animation sequences into");
		}

		ScriptStream stream(file);
		auto animations = parseAnincLines(stream);
		if (stream.hasNext())
		{
			throw mt_ex("Unexpected '" + stream.token() + "' in the animation sequences");
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

	std::string encodeAninc(const Model &source)
	{
		auto model = prepareForExport(source);

		std::ostringstream out;
		out << "\t\t// Exported with ModelTool\n";
		out << "\t\t// <animation name>\t<first frame> <length> <looping> <fps> <move speed> <transition> <reversed>\n\n";
		for (const auto &animation : model->animations)
		{
			if (animation.name.find_first_of(" \t\n") != std::string::npos)
			{
				throw mt_ex("Animation names can not contain spaces: " + animation.name);
			}
			out << "\t\t" << animation.name << "\t" << animation.startFrame << "\t" << animation.numFrames << "\t"
				<< (animation.loop ? 1 : 0) << "\t" << animation.fps << "\t" << animation.moveSpeed << "\t" << animation.transition << "\t"
				<< (animation.reversed ? 1 : 0) << "\n";
		}
		return out.str();
	}

	void writeAninc(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeAninc(model));
	}

	std::string encodeTags(const Model &source)
	{
		auto prepared = prepareForExport(source);
		auto *model = dynamic_cast<SkeletalModel *>(prepared.get());
		if (!model)
		{
			throw mt_ex("Tags files are written from skeletal models");
		}

		std::vector<BoneTag> tags = model->tags;
		for (size_t i = 0; i < model->joints.size(); i++)
		{
			if (model->joints[i].asTag && model->joints[i].parentId >= 0)
			{
				tags.push_back(boneTagFromJoint(*model, int(i)));
			}
		}

		std::ostringstream out;
		out << std::setprecision(9) << "// Exported with ModelTool\ntags {\n";
		for (const auto &tag : tags)
		{
			const auto &q = tag.rotation;
			out << "\t\"" << tag.name << "\" \"" << model->joints.at(size_t(tag.parentBone)).name << "\" ( " << tag.location.x << " "
				<< tag.location.y << " " << tag.location.z << " ) ( " << q.x << " " << q.y << " " << q.z << " " << q.w << " )\n";
		}
		out << "}\n";
		return out.str();
	}

	void writeTags(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeTags(model));
	}
}
