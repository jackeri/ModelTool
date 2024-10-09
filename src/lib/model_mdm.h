#include "model.h"
#include <ostream>

namespace mt::model::io::mdm {
	void writeModelMDM(std::ostream &os, const Model &model, const std::vector<Animation> &animations);
}
