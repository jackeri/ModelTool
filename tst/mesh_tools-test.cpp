#include "gtest/gtest.h"
#include "filesystem.h"
#include "mesh_tools.h"
#include "model_mdx.h"
#include "morph_model.h"
#include "posed_model.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"

#include <memory>

using namespace mt;
using namespace mt::model;

namespace {
	void expectVec(const glm::vec3 &actual, const glm::vec3 &expected, float tolerance = 1e-4f)
	{
		EXPECT_NEAR(actual.x, expected.x, tolerance);
		EXPECT_NEAR(actual.y, expected.y, tolerance);
		EXPECT_NEAR(actual.z, expected.z, tolerance);
	}

	// One triangle at x in two frames (the second one higher), with a normal pointing down (idTech winding)
	morph::Mesh triangle(const std::string &name, const std::string &material, float x)
	{
		morph::Mesh mesh;
		mesh.name = name;
		mesh.materialName = material;
		mesh.texCoords = {{0, 0}, {1, 0}, {0, 0.25f}};
		mesh.tris = {{0, 1, 2}};
		mesh.collapseMap = {0, 0, 0};
		mesh.minLod = 2;
		for (float z : {0.0f, 2.0f})
		{
			mesh.frames.push_back({{{x, 0, z}, {0, 0, -1}}, {{x + 1, 0, z}, {0, 0, -1}}, {{x, 1, z}, {0, 0, -1}}});
		}
		return mesh;
	}

	morph::MorphModel threeMeshes()
	{
		morph::MorphModel model;
		model.frames.resize(2);
		model.frames[0].bounds = {{0, 0, 0}, {6, 1, 0}};
		model.frames[0].localOrigin = {3, 0.5f, 0};
		model.meshes = {triangle("a", "shared", 0), triangle("b", "other", 2), triangle("c", "SHARED", 4)};
		model.lods.push_back(model.meshes);

		morph::Tag tag;
		tag.name = "tag_x";
		tag.frames = {{{1, 2, 3}, glm::angleAxis(glm::radians(30.0f), glm::vec3(0, 0, 1))}, {}};
		model.tags.push_back(tag);
		return model;
	}

	glm::vec3 triangleNormal(const morph::Mesh &mesh, size_t tri, size_t frame)
	{
		const auto &t = mesh.tris[tri];
		const auto &v = mesh.frames[frame];
		return glm::normalize(glm::cross(v[size_t(t.z)].pos - v[size_t(t.x)].pos, v[size_t(t.y)].pos - v[size_t(t.x)].pos));
	}
}

TEST(MeshToolsTest, MergeByMaterial)
{
	auto model = threeMeshes();
	model.meshes.push_back(triangle("d", "", 6)); // no material: named by a skin, not merged

	EXPECT_EQ(mergeMeshesByMaterial(model, 5, 100), 0); // would be over the vertex limit

	EXPECT_EQ(mergeMeshesByMaterial(model), 1);
	ASSERT_EQ(model.meshes.size(), 3);
	const auto &merged = model.meshes[0];
	EXPECT_EQ(merged.name, "a");
	EXPECT_EQ(merged.numVertices(), 6);
	ASSERT_EQ(merged.tris.size(), 2);
	EXPECT_EQ(merged.tris[1], glm::ivec3(3, 4, 5));
	EXPECT_EQ(merged.frames[1][3].pos, glm::vec3(4, 0, 2));
	EXPECT_TRUE(merged.collapseMap.empty());
	EXPECT_EQ(merged.minLod, 0);
	EXPECT_EQ(model.meshes[1].name, "b");
	EXPECT_EQ(model.meshes[2].name, "d");
	// The level of detail is merged too
	EXPECT_EQ(model.lods[0].size(), 2);
}

TEST(MeshToolsTest, RemoveMesh)
{
	auto model = threeMeshes();
	removeMesh(model, 1);
	ASSERT_EQ(model.meshes.size(), 2);
	EXPECT_EQ(model.meshes[1].name, "c");
	ASSERT_EQ(model.lods[0].size(), 2);
	EXPECT_EQ(model.lods[0][1].name, "c");
	EXPECT_THROW(removeMesh(model, 2), mt_exception);
	EXPECT_EQ(meshCount(model), 2);
}

TEST(MeshToolsTest, FlipAndWinding)
{
	auto model = threeMeshes();
	flipNormals(model, 1);
	EXPECT_EQ(model.meshes[1].frames[1][0].normal, glm::vec3(0, 0, 1));
	EXPECT_EQ(model.lods[0][1].frames[0][0].normal, glm::vec3(0, 0, 1));
	EXPECT_EQ(model.meshes[0].frames[0][0].normal, glm::vec3(0, 0, -1));

	reverseWinding(model, 0);
	EXPECT_EQ(model.meshes[0].tris[0], glm::ivec3(0, 2, 1));
	EXPECT_EQ(model.meshes[1].tris[0], glm::ivec3(0, 1, 2));

	flipTexCoords(model, -1, false, true);
	EXPECT_EQ(model.meshes[2].texCoords[2], glm::vec2(0, 0.75f));
	flipTexCoords(model, 0, true, false);
	EXPECT_EQ(model.meshes[0].texCoords[1], glm::vec2(0, 1));

	EXPECT_THROW(flipNormals(model, 5), mt_exception);
}

TEST(MeshToolsTest, MirrorMorph)
{
	auto model = threeMeshes();
	const glm::vec3 tagAxisX = model.tags[0].frames[0].rotation * glm::vec3(1, 0, 0);
	mirrorModel(model, 0);

	const auto &mesh = model.meshes[1];
	EXPECT_EQ(mesh.frames[1][1].pos, glm::vec3(-3, 0, 2));
	// Mirrored, the triangles still face where their normals point
	expectVec(triangleNormal(mesh, 0, 0), mesh.frames[0][0].normal);
	expectVec(triangleNormal(model.lods[0][2], 0, 1), {0, 0, -1});

	// Tags: the place and the axes are mirrored, the rotation stays a rotation
	const Point &tag = model.tags[0].frames[0];
	EXPECT_EQ(tag.location, glm::vec3(-1, 2, 3));
	expectVec(tag.rotation * glm::vec3(1, 0, 0), {tagAxisX.x, -tagAxisX.y, -tagAxisX.z});
	EXPECT_NEAR(glm::determinant(glm::mat3_cast(tag.rotation)), 1.0f, 1e-5f);

	EXPECT_EQ(model.frames[0].bounds.min, glm::vec3(-6, 0, 0));
	EXPECT_EQ(model.frames[0].bounds.max, glm::vec3(0, 1, 0));
	EXPECT_EQ(model.frames[0].localOrigin, glm::vec3(-3, 0.5f, 0));

	EXPECT_THROW(mirrorModel(model, 3), mt_exception);
}

TEST(MeshToolsTest, MirrorSkeletal)
{
	mt::io::FileSystem files;
	ASSERT_TRUE(files.addPath("data"));
	std::unique_ptr<Model> loaded(mt::model::io::loadModel(files.loadFile("models/mdm/mannequin.mdm")));
	loadMDXAnimation(loaded.get(), files.loadFile("models/mdm/mannequin.mdx"));
	auto &original = dynamic_cast<skeletal::SkeletalModel &>(*loaded);
	skeletal::SkeletalModel mirror = original;
	mirrorModel(mirror, 1);

	// In an animated frame every vertex and tag is the original one mirrored on Y
	PoseOptions options;
	options.frame = 70;
	options.nextFrame = 70;
	const auto before = poseModel(original, options);
	const auto after = poseModel(mirror, options);
	ASSERT_EQ(before.meshes.size(), after.meshes.size());
	for (size_t m = 0; m < before.meshes.size(); m++)
	{
		for (size_t v = 0; v < before.meshes[m].positions.size(); v += 17)
		{
			const glm::vec3 p = before.meshes[m].positions[v];
			expectVec(after.meshes[m].positions[v], {p.x, -p.y, p.z}, 1e-3f);
			const glm::vec3 n = before.meshes[m].normals[v];
			expectVec(after.meshes[m].normals[v], {n.x, -n.y, n.z}, 1e-3f);
		}
	}
	glm::mat4 tagBefore{1.0f}, tagAfter{1.0f};
	ASSERT_TRUE(tagMatrix(before, "tag_weapon", tagBefore));
	ASSERT_TRUE(tagMatrix(after, "tag_weapon", tagAfter));
	const glm::vec3 place = tagBefore[3];
	expectVec(glm::vec3(tagAfter[3]), {place.x, -place.y, place.z}, 1e-3f);
	// Faces point outward: the winding is reversed with the mirror
	EXPECT_EQ(mirror.meshes[0].tris[0],
			  glm::ivec3(original.meshes[0].tris[0].x, original.meshes[0].tris[0].z, original.meshes[0].tris[0].y));
}
