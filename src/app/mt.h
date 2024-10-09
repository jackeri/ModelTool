#pragma once

#include "../lib/library.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <spdlog/spdlog.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#ifndef BIT
#define BIT(x) (1u << (x))
#endif

namespace mt {

	const std::string MT_LOGGER = "mt"; ///< Used logger name
}
