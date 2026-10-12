#include "state.h"
#include "skeletal_model.h"

namespace mt {

	namespace {
		std::shared_ptr<spdlog::logger> log()
		{
			return spdlog::get(MT_LOGGER);
		}

		std::string fileName(const std::string &path)
		{
			auto slash = path.find_last_of("/\\");
			std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			return name;
		}

		model::Animation findSequence(const std::vector<model::Animation> &list, const std::string &name)
		{
			auto found = std::find_if(list.begin(), list.end(), [&](const auto &animation) { return animation.name == name; });
			return found != list.end() ? *found : (list.empty() ? model::Animation{} : list.front());
		}
	}

	Ref<Model> State::loadModelFile(const std::string &path)
	{
		Ref<Model> loaded(model::io::loadModel(filesystem.loadFile(path)));

		if (auto *morph = dynamic_cast<model::morph::MorphModel *>(loaded.get()))
		{
			for (const auto &problem : model::loadMorphLods(*morph, filesystem, path))
			{
				if (auto logger = log())
				{
					logger->warn("{}", problem);
				}
			}
		}
		return loaded;
	}

	void State::loadModel(const std::string &path)
	{
		model::PlayerFiles files;
		if (fileName(path) == "animation.cfg")
		{
			if (!model::findPlayerFiles(filesystem, path, files))
			{
				throw mt_ex("No player model (lower.md3, upper.md3 and head.md3) next to " + path);
			}
			loadPlayer(files);
			return;
		}

		loadSingleModel(path, true);
	}

	void State::loadPlayer(const model::PlayerFiles &files)
	{
		auto config = model::parsePlayerConfig(model::fileText(filesystem.loadFile(files.config)));

		loadSingleModel(files.lower, false);
		settings.addRecentFile(files.config);
		model->animations = config.legsAnimations();

		try
		{
			Attachment &upper = attachModel(files.upper, -1, "tag_torso");
			upper.name = "upper";
			upper.model->animations = config.torsoAnimations();
			attachModel(files.head, 0, "tag_head").name = "head";
		}
		catch (const std::exception &)
		{
			closeModels();
			throw;
		}

		isPlayer = true;
		player = files;
		playerConfig = config;
		setPlayerSkin(files.skins.empty() ? std::string() : files.skins.front());

		playSequence(-1, findSequence(model->animations, "LEGS_IDLE"));
		playSequence(0, findSequence(attachments[0].model->animations, "TORSO_STAND"));
		playback.pause();
	}

	void State::setPlayerSkin(const std::string &name)
	{
		playerSkin = name;
		auto load = [&](const std::string &part) {
			if (name.empty())
			{
				return model::Skin{};
			}
			const std::string path = player.skinPath(part, name);
			try
			{
				return model::loadSkin(filesystem.loadFile(path));
			}
			catch (const std::exception &ex)
			{
				if (auto logger = log())
				{
					logger->warn("Skin {}: {}", path, ex.what());
				}
				return model::Skin{};
			}
		};

		skin = load("lower");
		if (attachments.size() >= 2)
		{
			attachments[0].skin = load("upper");
			attachments[1].skin = load("head");
		}
	}

	void State::playSequence(int target, const model::Animation &animation)
	{
		Model *targetModel = target < 0 ? model.get() : (target < int(attachments.size()) ? attachments[target].model.get() : nullptr);
		if (!targetModel || !targetModel->hasFrames())
		{
			return;
		}

		model::Playback &targetPlayback = target < 0 ? playback : attachments[target].playback;
		targetPlayback.setAnimation(animation);
		targetPlayback.seek(animation.startFrame);
		targetModel->currentFrame = std::clamp(targetPlayback.frame(), 0, targetModel->numFrames() - 1);
	}

	Attachment &State::attachModel(const std::string &path, int parent, const std::string &tag)
	{
		if (!model)
		{
			throw mt_ex("Open a model first");
		}
		if (parent >= int(attachments.size()))
		{
			throw mt_ex("Invalid parent for the attachment");
		}

		Attachment attachment;
		attachment.model = loadModelFile(path);
		attachment.path = path;
		attachment.name = path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1);
		attachment.parent = parent;
		attachment.tag = tag;

		const int frames = attachment.model->numFrames();
		attachment.playback.setRange(0, std::max(frames - 1, 0), 24.0f, true);
		attachment.model->currentFrame = frames > 0 ? 0 : FRAME_BIND_POSE;

		attachments.push_back(std::move(attachment));
		return attachments.back();
	}

	void State::removeAttachment(int index)
	{
		if (index < 0 || index >= int(attachments.size()))
		{
			return;
		}

		// The removed attachment and everything on it
		std::vector<bool> removed(attachments.size(), false);
		for (size_t i = size_t(index); i < attachments.size(); i++)
		{
			removed[i] = int(i) == index || (attachments[i].parent >= 0 && removed[size_t(attachments[i].parent)]);
		}

		std::vector<int> newIndex(attachments.size(), -1);
		std::vector<Attachment> kept;
		for (size_t i = 0; i < attachments.size(); i++)
		{
			if (!removed[i])
			{
				newIndex[i] = int(kept.size());
				kept.push_back(std::move(attachments[i]));
			}
		}
		for (auto &attachment : kept)
		{
			attachment.parent = attachment.parent < 0 ? -1 : newIndex[size_t(attachment.parent)];
		}
		attachments = std::move(kept);

		// Not a complete player any more
		if (isPlayer && attachments.size() < 2)
		{
			isPlayer = false;
		}
	}

	std::vector<std::string> State::tagNames(int target) const
	{
		const Model *targetModel
			= target < 0 ? model.get() : (target < int(attachments.size()) ? attachments[target].model.get() : nullptr);
		std::vector<std::string> names;
		if (const auto *morph = dynamic_cast<const model::morph::MorphModel *>(targetModel))
		{
			for (const auto &tag : morph->tags)
			{
				names.push_back(tag.name);
			}
		}
		else if (const auto *skeletal = dynamic_cast<const model::skeletal::SkeletalModel *>(targetModel))
		{
			for (const auto &tag : skeletal->tags)
			{
				names.push_back(tag.name);
			}
			for (const auto &joint : skeletal->joints)
			{
				if (joint.asTag)
				{
					names.push_back(joint.name);
				}
			}
		}
		return names;
	}

	std::vector<std::string> State::attachPoints(int target) const
	{
		auto points = tagNames(target);
		const Model *targetModel
			= target < 0 ? model.get() : (target < int(attachments.size()) ? attachments[target].model.get() : nullptr);
		if (const auto *skeletal = dynamic_cast<const model::skeletal::SkeletalModel *>(targetModel))
		{
			for (const auto &joint : skeletal->joints)
			{
				if (!joint.asTag)
				{
					points.push_back(joint.name);
				}
			}
		}
		return points;
	}

	void State::update(double seconds)
	{
		if (!model)
		{
			return;
		}

		const bool playing = playback.isPlaying() && model->hasFrames();
		if (playing)
		{
			playback.advance(seconds);
			model->currentFrame = std::clamp(playback.frame(), 0, model->numFrames() - 1);
		}

		// Attachments play their own frames along with the main playback controls
		for (auto &attachment : attachments)
		{
			if (!attachment.model->hasFrames())
			{
				continue;
			}
			attachment.playback.speed = playback.speed;
			attachment.playback.interpolate = playback.interpolate;
			if (playing != attachment.playback.isPlaying())
			{
				playing ? attachment.playback.play() : attachment.playback.pause();
			}
			attachment.playback.advance(seconds);
			attachment.model->currentFrame = std::clamp(attachment.playback.frame(), 0, attachment.model->numFrames() - 1);
		}
	}
}
