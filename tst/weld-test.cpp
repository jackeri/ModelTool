#include "gtest/gtest.h"
#include "morph_model.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "weld.h"

using namespace mt;
using namespace mt::model;

namespace {
	// Two triangles side by side with a 0.05 gap at their shared edge, in two frames (the second moved up)
	morph::Mesh gapTriangle(float x)
	{
		morph::Mesh mesh;
		mesh.texCoords.resize(3);
		mesh.tris = {{0, 1, 2}};
		for (float z : {0.0f, 4.0f})
		{
			mesh.frames.push_back({{{x, 0, z}, {}}, {{x, 1, z}, {}}, {{x + 1, 0, z}, {}}});
		}
		return mesh;
	}

	morph::MorphModel gapModel(bool oneMesh)
	{
		morph::MorphModel model;
		model.frames.resize(2);
		auto left = gapTriangle(0.0f);
		auto right = gapTriangle(-1.05f);
		if (oneMesh)
		{
			// Both triangles in one mesh
			for (size_t f = 0; f < 2; f++)
			{
				left.frames[f].insert(left.frames[f].end(), right.frames[f].begin(), right.frames[f].end());
			}
			left.texCoords.resize(6);
			left.tris.push_back({3, 4, 5});
			model.meshes.push_back(left);
		}
		else
		{
			model.meshes.push_back(left);
			model.meshes.push_back(right);
		}
		return model;
	}
}

TEST(WeldTest, MorphInOneMesh)
{
	auto model = gapModel(true);

	// Too far apart
	auto result = weldVertices(model, 0.01f);
	EXPECT_EQ(result.groups, 0);

	result = weldVertices(model, 0.1f);
	EXPECT_EQ(result.groups, 1);
	EXPECT_EQ(result.moved, 2);
	const auto &mesh = model.meshes[0];
	for (size_t f = 0; f < 2; f++)
	{
		// The gap vertices (0 and 5) meet halfway, in every frame; the others stay
		EXPECT_EQ(mesh.frames[f][0].pos, mesh.frames[f][5].pos);
		EXPECT_NEAR(mesh.frames[f][0].pos.x, -0.025f, 1e-6f);
		EXPECT_EQ(mesh.frames[f][1].pos, glm::vec3(0, 1, f * 4.0f));
	}
	EXPECT_EQ(mesh.numVertices(), 6);
}

TEST(WeldTest, MorphAcrossMeshes)
{
	auto model = gapModel(false);
	model.lods.push_back(model.meshes);

	EXPECT_EQ(weldVertices(model, 0.1f).groups, 0);

	auto result = weldVertices(model, 0.1f, true);
	EXPECT_EQ(result.groups, 2); // the base level and the level of detail
	EXPECT_EQ(model.meshes[0].frames[1][0].pos, model.meshes[1].frames[1][2].pos);
	EXPECT_EQ(model.lods[0][0].frames[0][0].pos, model.lods[0][1].frames[0][2].pos);

	EXPECT_THROW(weldVertices(model, -1.0f), mt_exception);
}

TEST(WeldTest, SkeletalSharesWeights)
{
	// Two meshes on two joints, with a vertex of each 0.05 apart
	skeletal::SkeletalModel model;
	model.hasBindPose = true;
	for (const char *name : {"a", "b"})
	{
		skeletal::Joint joint;
		joint.name = name;
		joint.parentId = -1;
		model.joints.push_back(joint);
	}
	model.joints[1].location = {2, 0, 0};

	auto addMesh = [&](int joint, const std::vector<glm::vec3> &positions) {
		skeletal::Mesh mesh;
		mesh.tris = {{0, 1, 2}};
		for (const auto &pos : positions)
		{
			skeletal::SkeletalVertex vert;
			vert.pos = pos;
			vert.startWeight = int(mesh.weights.size());
			vert.weightCount = 1;
			mesh.vertices.push_back(vert);
			mesh.weights.push_back({joint, 1.0f, pos - model.joints[size_t(joint)].location});
		}
		model.meshes.push_back(mesh);
	};
	addMesh(0, {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
	addMesh(1, {{1.05f, 0, 0}, {3, 0, 0}, {2, 1, 0}});

	auto result = weldVertices(model, 0.1f, true);
	EXPECT_EQ(result.groups, 1);
	EXPECT_EQ(result.moved, 1);
	EXPECT_EQ(result.reweighted, 1);

	// The second mesh's vertex takes the weights of the first one: the same place in the bind pose and in a frame
	// where the joints move apart
	const auto &moved = model.meshes[1].vertices[0];
	ASSERT_EQ(moved.weightCount, 1);
	EXPECT_EQ(model.meshes[1].weights[size_t(moved.startWeight)].jointId, 0);
	EXPECT_EQ(model.meshes[1].weights.size(), 3);
	for (auto &joint : model.joints)
	{
		joint.frames.push_back(joint);
	}
	model.joints[0].frames[0].location = {0, 0, 5};
	for (int frame : {FRAME_BIND_POSE, 0})
	{
		EXPECT_EQ(skeletal::posedVertex(model, model.meshes[0], model.meshes[0].vertices[1], frame),
				  skeletal::posedVertex(model, model.meshes[1], moved, frame));
	}
	// The other vertices keep their weights
	EXPECT_EQ(skeletal::posedVertex(model, model.meshes[1], model.meshes[1].vertices[1], FRAME_BIND_POSE), glm::vec3(3, 0, 0));
}
