#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"

namespace mt::model {
	Model* loadMd5Model(const Ref<IO::MTFile> &);
	void loadMD5Anim(Model *, const Ref<IO::MTFile> &);
}
