#include "model_mdm.h"
#include "model_types.h"
#include "skeletal_model.h"
#include <algorithm>
#include <cassert>
#include <cstring>

using namespace mt::model;

void io::mdm::writeModelMDM(std::ostream &os, const Model &model, const std::vector<Animation> &animations)
{
	auto &mdl = reinterpret_cast<const skeletal::SkeletalModel &>(model);
	mdmHeader_t header;
	header.ident = MDM_IDENT;
	header.version = MDM_VERSION;
	std::fill_n(header.name, sizeof(header.name), 0);
	mdl.name.copy(header.name, sizeof(header.name) - 1);

	// These seem to be the default values (based on the original models)
	header.lodScale = 1.f;
	header.lodBias = 0.f;

	header.numSurfaces = static_cast<i32>(mdl.meshes.size());
	header.numTags = static_cast<i32>(
		mdl.tags.size() + std::count_if(mdl.joints.begin(), mdl.joints.end(), [](const skeletal::Joint &joint) { return joint.asTag; }));

	header.ofsSurfaces = 0;
	header.ofsTags = 0;
	header.ofsEnd = 0;

	os.write(reinterpret_cast<const char *>(&header), sizeof(mdmHeader_t));

	header.ofsSurfaces = static_cast<i32>(os.tellp());
	for (const auto &mesh : mdl.meshes)
	{
		mdmSurface_t surface;

		surface.ident = 0;
		std::fill_n(surface.name, sizeof(surface.name), 0);
		mesh.name.copy(surface.name, sizeof(surface.name) - 1);

		std::fill_n(surface.shader, sizeof(surface.shader), 0);
		mesh.materialName.copy(surface.shader, sizeof(surface.shader) - 1);
		surface.shaderIndex = 0; // Only used in runtime, zero it out

		surface.minLod = mesh.minLod;

		surface.ofsHeader = -static_cast<i32>(os.tellp());

		surface.numVerts = static_cast<i32>(mesh.vertices.size());
		surface.ofsVerts = 0;

		surface.numTriangles = static_cast<i32>(mesh.tris.size());
		surface.ofsTriangles = 0;

		surface.ofsCollapseMap = 0;

		surface.numBoneReferences = 0;
		surface.ofsBoneReferences = 0;

		surface.ofsEnd = 0;

		os.write(reinterpret_cast<const char *>(&surface), sizeof(mdcSurface_t));

		i32 surfaceOffset = static_cast<i32>(os.tellp());

		surface.ofsTriangles = static_cast<i32>(os.tellp()) - surfaceOffset;
		for (const auto &tri : mesh.tris)
		{
			mdmTriangle_t triangle{tri.x, tri.y, tri.z};
			os.write(reinterpret_cast<const char *>(&triangle), sizeof(mdmTriangle_t));
		}

		surface.ofsVerts = static_cast<i32>(os.tellp());
		for (const auto &vert : mesh.vertices)
		{
			mdmVertex_t vertex;
			std::memset(&vertex, 0, sizeof(mdmVertex_t));

			glm_to_vec3(vert.normal, vertex.normal);
			glm_to_vec2(vert.tex, vertex.texCoords);
			vertex.numWeights = vert.weightCount;
			assert(vert.weightCount >= 1 && vert.weightCount <= 5);

			std::size_t ss = sizeof(vertex.normal);
			assert(ss == sizeof(f32) * 3 && ss == 12);

			os.write(reinterpret_cast<const char *>(&vertex.normal), sizeof(f32) * 3);
			os.write(reinterpret_cast<const char *>(&vertex.texCoords), sizeof(f32) * 2);
			os.write(reinterpret_cast<const char *>(&vertex.numWeights), sizeof(i32));

			for (int i = 0; i < vert.weightCount; i++)
			{
				auto weight = mesh.weights[vert.startWeight + i];

				mdmWeight_t w;
				std::memset(&w, 0, sizeof(mdmWeight_t));

				// FIXME: handle cases where we are turning joints into tags and they wont be joints anymore
				w.boneIndex = weight.jointId;
				w.boneWeight = weight.bias;
				glm_to_vec3(weight.pos, w.offset);

				os.write(reinterpret_cast<const char *>(&w), sizeof(mdmWeight_t));
			}

			os.write(reinterpret_cast<const char *>(&vertex), sizeof(mdmVertex_t));
		}

		surface.ofsCollapseMap = static_cast<i32>(os.tellp());
		assert(mesh.collapseMap.size());
		for (auto &cm : mesh.collapseMap)
		{
			i32 val = static_cast<i32>(cm);
			os.write(reinterpret_cast<const char *>(&val), sizeof(i32));
		}
	}
}
