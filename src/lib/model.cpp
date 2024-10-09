#include "model.h"
#include "model_md5.h"
#include <array>

namespace mt::model::io {

	using ModelLoaderMap = std::unordered_map<std::string, const std::function<Model *(const Ref<mt::io::MTFile> &)>>;
	using AnimationLoaderMap = std::unordered_map<std::string, const std::function<void(Model *, const Ref<mt::io::MTFile> &)>>;

	using ModelWriterMap = std::unordered_map<std::string, const std::function<void(std::ostream &, const Model &)>>;
	using AnimationWriterMap = std::unordered_map<std::string, const std::function<void(Model *, const Ref<mt::io::MTFile> &)>>;

	static ModelLoaderMap m_modelLoaders{{".md5mesh", model::loadMd5Model}};
	static AnimationLoaderMap m_animationLoaders{{".md5anim", model::loadMD5Anim}};

	Model *loadModel(const Ref<mt::io::MTFile> &file)
	{
		auto it
			= std::find_if(std::begin(m_modelLoaders), std::end(m_modelLoaders), [&](auto &&loader) { return loader.first == file->ext; });

		if (it == std::end(m_modelLoaders))
		{
			throw std::invalid_argument("Unknown extension.");
		}

		return it->second(file);
	}

	void loadAnimation(Model *parent, const Ref<mt::io::MTFile> &file)
	{
		auto it = std::find_if(std::begin(m_animationLoaders), std::end(m_animationLoaders),
							   [&](auto &&loader) { return loader.first == file->ext; });

		if (it == std::end(m_animationLoaders))
		{
			throw std::invalid_argument("Unknown extension.");
		}

		return it->second(parent, file);
	}

	void writeModel(std::ostream &os, const Model &model, const std::vector<Animation> &animations) {}

	void writeAnimation(std::ostream &os, const Model &model, const std::vector<Animation> &animations) {}
}
