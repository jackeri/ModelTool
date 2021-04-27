#pragma once

#include "mt.h"
#include "Texture.h"

namespace mt {
	class Material {
	public:
		std::string name;
		Texture *texture;
	};
}