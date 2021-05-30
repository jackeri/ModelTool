#include "ModelLoader.h"
#include "ModelMD5.h"

namespace mt {

	namespace model {
		ModelLoaderMap ModelLoader::m_modelLoaders{};
		AnimationLoaderMap ModelLoader::m_animationLoaders{};
	}

	Model *model::ModelLoader::loadModel(const Ref<IO::MTFile> &file)
	{
		// Make sure the loader map is initialized
		init();

		auto it = std::find_if(std::begin(m_modelLoaders), std::end(m_modelLoaders),
							   [&](auto &&loader) { return loader.first == file->ext; });

		if (it == std::end(m_modelLoaders))
		{
			throw std::invalid_argument("Unknown extension.");
		}

		return it->second(file);
	}

	Ref<Model> model::ModelLoader::loadModel_ref(const Ref<IO::MTFile> &file)
	{
		return Ref<Model>(loadModel(file));
	}

	void model::ModelLoader::loadAnimation(Model *parent, const Ref<IO::MTFile> &file)
	{
		init();

		auto it = std::find_if(std::begin(m_animationLoaders), std::end(m_animationLoaders),
							   [&](auto &&loader) { return loader.first == file->ext; });

		if (it == std::end(m_animationLoaders))
		{
			throw std::invalid_argument("Unknown extension.");
		}

		return it->second(parent, file);
	}

	void model::ModelLoader::loadAnimation(const Ref<Model> &parent, const Ref<IO::MTFile> &file)
	{
		loadAnimation(parent.get(), file);
	}

	void model::ModelLoader::init()
	{
		if (!m_modelLoaders.empty())
		{
			return;
		}

		// MD5 model and animations loaders setup
		m_modelLoaders.emplace("md5mesh", model::loadMd5Model);
		m_animationLoaders.emplace("md5anim", model::loadMD5Anim);

		// FIXME: register the the missing model formats here
	}
}
