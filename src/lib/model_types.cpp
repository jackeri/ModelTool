#include <glm/glm.hpp>
#include "model_types.h"

void vec2_to_glm(const vec2_t in, glm::vec2 &out)
{
	out.x = in[0];
	out.y = in[1];
}

void glm_to_vec2(const glm::vec2 &in, vec2_t out)
{
	out[0] = in.x;
	out[1] = in.y;
}

void vec3_to_glm(const vec3_t in, glm::vec3 &out)
{
	out.x = in[0];
	out.y = in[1];
	out.z = in[2];
}

void glm_to_vec3(const glm::vec3 &in, vec3_t out)
{
	out[0] = in.x;
	out[1] = in.y;
	out[2] = in.z;
}
