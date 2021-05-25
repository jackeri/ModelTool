#include "ModelLoader.h"
#include "ModelMD5.h"

mt::Model *mt::model::ModelLoader::load(const mt::Ref<mt::IO::MTFile> &)
{
	init();

	return nullptr;
}

void mt::model::ModelLoader::init()
{
	if (!ModelLoader::loaders.empty()) {
		return;
	}

	ModelLoader::loaders.emplace(std::string("md5"), mt::model::loadMD5);

	// FIXME: register the the missing model formats here
}
