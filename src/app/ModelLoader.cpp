#include "ModelLoader.h"
#include "ModelMD5.h"

namespace mt {

	Model *model::ModelLoader::load(const Ref<IO::MTFile> &file)
	{
		// Make sure the loader map is initialized
		init();

		auto it = std::find_if(std::begin(m_loaders), std::end(m_loaders),
							   [&](auto &&loader) { return loader.first == file->ext; });

		if (it == std::end(m_loaders))
		{
			throw std::invalid_argument("Unknown extension.");
		}

		return it->second(file);
	}

	Ref<Model> model::ModelLoader::load_ref(const Ref<IO::MTFile> &file)
	{
		return Ref<Model>(load(file));
	}

	void model::ModelLoader::init()
	{
		if (!m_loaders.empty())
		{
			return;
		}

		m_loaders.emplace(std::string("md5"), model::loadMD5);

		// FIXME: register the the missing model formats here
	}

}
