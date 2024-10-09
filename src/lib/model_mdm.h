#include "model.h"
#include <ostream>

namespace mt::model::io {
	void writeModelMDM(std::ostream &os, const Model &model, const std::vector<Animation> &animations);
}
