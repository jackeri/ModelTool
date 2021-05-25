#include "ModelLoader.h"
#include "ModelMD5.h"

mt::Model *mt::model::ModelLoader::load(const mt::Ref<mt::IO::MTFile> &file)
{
	// Make sure the loader map is initialized
	init();

	auto it = std::find_if(std::begin(loaders), std::end(loaders),
						   [&](auto &&loader) { return loader.first == file->ext; });

	if (it == std::end(loaders))
	{
		throw std::invalid_argument("Unknown extension.");
	}

	return it->second(file);
}

void mt::model::ModelLoader::init()
{
	if (!loaders.empty())
	{
		return;
	}

	loaders.emplace(std::string("md5"), mt::model::loadMD5);

	// FIXME: register the the missing model formats here
}
