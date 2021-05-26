#include "ModelMD5.h"
#include "SkeletalModel.h"
#include "ScriptStream.h"

namespace mt::model {

	using namespace Skeletal;

	Model *loadMD5(const Ref<IO::MTFile> &file)
	{
		// MD5 model file is just a text file
		ScriptStream stream(file);

		auto *model = new SkeletalModel;

		return model;
	}

}
