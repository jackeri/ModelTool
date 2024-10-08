#pragma once

#include <utility>

#include "mt.h"
#include "texture.h"

namespace mt {

	/*! Material, not used currently but this will hold the parsable material script and loaded textures */
	class Material {
	  public:
		Material() = default;

		Material(std::string materialName) : name(std::move(materialName)) {}

		std::string name{};			///< Name of the material
		Texture *texture = nullptr; ///< Loaded texture instance
	};
}