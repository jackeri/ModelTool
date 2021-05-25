#pragma once

#include "mt.h"
#include "SkeletalModel.h"
#include "filesystem.h"

namespace mt::model {
	class ModelMD5 : public Skeletal::SkeletalModel {
	public:

		friend Model* loadMD5(const Ref<IO::MTFile> &);
	};

	Model* loadMD5(const Ref<IO::MTFile> &);
}
