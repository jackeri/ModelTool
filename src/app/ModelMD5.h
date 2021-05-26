#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"

namespace mt::model {
	Model* loadMD5(const Ref<IO::MTFile> &);
}
