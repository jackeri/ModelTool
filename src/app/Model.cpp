#include "Model.h"
#include "renderer.h"

namespace mt {
	void setupBounds(const Ref<mt::Renderer> &renderer, const Bounds &bounds, bool drawPoints)
	{
		auto &buffer = renderer->getBuffer();
		buffer.clear(false);

		buffer.addXyz({bounds.max.x, bounds.min.y, bounds.max.z}); // 2
		buffer.addXyz({bounds.min.x, bounds.min.y, bounds.max.z}); // 3

		buffer.addXyz({bounds.min.x, bounds.min.y, bounds.max.z}); // 3
		buffer.addXyz({bounds.min.x, bounds.min.y, bounds.min.z}); // 0

		buffer.addXyz({bounds.min.x, bounds.min.y, bounds.max.z}); // 3
		buffer.addXyz({bounds.min.x, bounds.max.y, bounds.max.z}); // 4

		buffer.addXyz({bounds.min.x, bounds.max.y, bounds.max.z}); // 4
		buffer.addXyz({bounds.min.x, bounds.max.y, bounds.min.z}); // 7

		buffer.addXyz({bounds.min.x, bounds.max.y, bounds.max.z}); // 4
		buffer.addXyz({bounds.max.x, bounds.max.y, bounds.max.z}); // 5

		buffer.addXyz({bounds.max.x, bounds.max.y, bounds.max.z}); // 5
		buffer.addXyz({bounds.max.x, bounds.min.y, bounds.max.z}); // 2

		buffer.addXyz({bounds.max.x, bounds.max.y, bounds.max.z}); // 5
		buffer.addXyz({bounds.max.x, bounds.max.y, bounds.min.z}); // 6

		buffer.addXyz({bounds.max.x, bounds.max.y, bounds.min.z}); // 6
		buffer.addXyz({bounds.max.x, bounds.min.y, bounds.min.z}); // 1

		buffer.addXyz({bounds.max.x, bounds.max.y, bounds.min.z}); // 6
		buffer.addXyz({bounds.min.x, bounds.max.y, bounds.min.z}); // 7

		buffer.addXyz({bounds.min.x, bounds.min.y, bounds.min.z}); // 0
		buffer.addXyz({bounds.max.x, bounds.min.y, bounds.min.z}); // 1

		buffer.addXyz({bounds.max.x, bounds.min.y, bounds.min.z}); // 1
		buffer.addXyz({bounds.max.x, bounds.min.y, bounds.max.z}); // 2

		buffer.addXyz({bounds.min.x, bounds.max.y, bounds.min.z}); // 7
		buffer.addXyz({bounds.min.x, bounds.min.y, bounds.min.z}); // 0

		// FIXME: implement point drawing
	}
}
