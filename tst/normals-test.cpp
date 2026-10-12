#include "gtest/gtest.h"
#include "normals.h"
#include "model_md5.h"
#include "mt_path.h"

using namespace mt;
using namespace mt::model;

namespace {
	void expectVec(const glm::vec3 &actual, const glm::vec3 &expected, float tolerance = 1e-4f)
	{
		EXPECT_NEAR(actual.x, expected.x, tolerance);
		EXPECT_NEAR(actual.y, expected.y, tolerance);
		EXPECT_NEAR(actual.z, expected.z, tolerance);
	}

	// Rotated normal of a vertex in a pose, the same way the renderer calculates it
	glm::vec3 posedNormal(const skeletal::SkeletalModel &model, const skeletal::Mesh &mesh, const skeletal::SkeletalVertex &vert, int frame)
	{
		glm::vec3 normal{0, 0, 0};
		for (int i = 0; i < vert.weightCount; i++)
		{
			const auto &weight = mesh.weights[vert.startWeight + i];
			const auto &joint = model.joints[weight.jointId];
			const Point &pose = frame == FRAME_BIND_POSE ? static_cast<const Point &>(joint) : joint.frames[frame];
			normal += (pose.rotation * vert.normal) * weight.bias;
		}
		return glm::normalize(normal);
	}

	// Flat quad in the xy plane, half bound to a rotated joint
	skeletal::SkeletalModel makeSkeletalQuad()
	{
		skeletal::SkeletalModel model;
		model.hasBindPose = true;

		skeletal::Joint root;
		root.name = "root";
		root.parentId = -1;
		model.joints.push_back(root);

		skeletal::Joint rotated;
		rotated.name = "rotated";
		rotated.parentId = -1;
		rotated.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{1, 0, 0});
		model.joints.push_back(rotated);

		skeletal::Mesh mesh;
		mesh.tris = {{0, 1, 2}, {0, 2, 3}};

		// Weights are joint local, the rotated joint gets the inverse rotated positions so the bind pose is flat
		std::vector<glm::vec3> positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
		for (size_t i = 0; i < positions.size(); i++)
		{
			skeletal::SkeletalVertex vert;
			vert.startWeight = int(mesh.weights.size());
			vert.weightCount = 2;
			vert.pos = positions[i];
			mesh.vertices.push_back(vert);

			mesh.weights.push_back({0, 0.5f, positions[i]});
			mesh.weights.push_back({1, 0.5f, glm::inverse(rotated.rotation) * positions[i]});
		}

		model.meshes.push_back(mesh);
		return model;
	}
}

TEST(NormalsTest, skeletalBindPose)
{
	auto model = makeSkeletalQuad();
	ASSERT_TRUE(calculateNormals(model));

	auto &mesh = model.meshes[0];
	for (auto &vert : mesh.vertices)
	{
		expectVec(vert.bindNormal, {0, 0, -1});
		// The joint local normal turned back with the joint rotations gives the bind pose normal
		expectVec(posedNormal(model, mesh, vert, FRAME_BIND_POSE), vert.bindNormal);
	}
}

TEST(NormalsTest, skeletalWithoutPose)
{
	skeletal::SkeletalModel model;
	ASSERT_FALSE(calculateNormals(model));
}

TEST(NormalsTest, skeletalInvalidWeight)
{
	auto model = makeSkeletalQuad();
	model.meshes[0].weights[0].jointId = 5;
	ASSERT_THROW(calculateNormals(model), mt_exception);
}

TEST(NormalsTest, morphSmoothAndFlat)
{
	morph::MorphModel model;
	model.frames.resize(1);

	// Two triangles folded 90 degrees along the x axis
	morph::Mesh mesh;
	mesh.texCoords.resize(4);
	mesh.tris = {{0, 1, 2}, {0, 3, 1}};
	mesh.frames.push_back({{{0, 0, 0}, {}}, {{1, 0, 0}, {}}, {{0, 1, 0}, {}}, {{0, 0, 1}, {}}});
	model.meshes.push_back(mesh);
	model.lods.push_back(model.meshes);

	calculateNormals(model, NormalMode::Smooth);
	auto &frame = model.meshes[0].frames[0];
	expectVec(frame[2].normal, {0, 0, -1});
	expectVec(frame[3].normal, {0, -1, 0});
	// Shared vertices get the average
	expectVec(frame[0].normal, glm::normalize(glm::vec3{0, -1, -1}));
	// Levels of detail are calculated too
	expectVec(model.lods[0][0].frames[0][0].normal, glm::normalize(glm::vec3{0, -1, -1}));

	calculateNormals(model, NormalMode::Flat);
	// The last triangle using the vertex wins
	expectVec(frame[0].normal, {0, -1, 0});
	expectVec(frame[2].normal, {0, 0, -1});
}

TEST(NormalsTest, md5NormalsMatchBindPose)
{
	mt::io::MTPath path("data");
	std::unique_ptr<Model> loaded(loadMd5Model(path.loadFile("models/md5/bob_lamp_update_export.md5mesh")));
	auto *model = dynamic_cast<skeletal::SkeletalModel *>(loaded.get());
	ASSERT_TRUE(model);

	size_t checked = 0;
	for (auto &mesh : model->meshes)
	{
		for (auto &vert : mesh.vertices)
		{
			if (glm::length(vert.bindNormal) == 0.0f)
			{
				continue; // vertex without triangles
			}

			ASSERT_NEAR(glm::length(vert.bindNormal), 1.0f, 1e-4f);
			expectVec(posedNormal(*model, mesh, vert, FRAME_BIND_POSE), vert.bindNormal, 1e-3f);
			checked++;
		}
	}
	ASSERT_GT(checked, 0);
}
