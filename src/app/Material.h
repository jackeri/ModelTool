#pragma once

#include <utility>

#include "mt.h"
#include "Texture.h"

namespace mt {
	class Material {
	public:
		Material() = default;

		Material(std::string materialName) : name(std::move(materialName))
		{}

		std::string name{};
		Texture *texture = nullptr;
	};
}