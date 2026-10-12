#include "gtest/gtest.h"
#include "lod.h"
#include "posed_model.h"
#include "skeletal_tools.h"
#include "et_armature.h"
#include "morph_model.h"
#include "mt_path.h"

#include <numeric>
#include <set>

using namespace mt;
using namespace mt::model;
using namespace mt::model::skeletal;

namespace {
	void expectVec(const glm::vec3 &actual, const glm::vec3 &expected, float tolerance = 1e-3f)
	{
		EXPECT_NEAR(actual.x, expected.x, tolerance);
		EXPECT_NEAR(actual.y, expected.y, tolerance);
		EXPECT_NEAR(actual.z, expected.z, tolerance);
	}

	std::unique_ptr<SkeletalModel> load(const std::string &path, const std::string &animation = "")
	{
		mt::io::MTPath data("data");
		std::unique_ptr<Model> loaded(model::io::loadModel(data.loadFile(path)));
		if (!animation.empty())
		{
			model::io::loadAnimation(loaded.get(), data.loadFile(animation));
		}
		return std::unique_ptr<SkeletalModel>(dynamic_cast<SkeletalModel *>(loaded.release()));
	}

	/*! Flat n x n grid of vertices */
	void grid(int n, std::vector<glm::vec3> &positions, std::vector<glm::ivec3> &tris)
	{
		for (int y = 0; y < n; y++)
		{
			for (int x = 0; x < n; x++)
			{
				positions.emplace_back(float(x), float(y), 0.0f);
			}
		}
		for (int y = 0; y < n - 1; y++)
		{
			for (int x = 0; x < n - 1; x++)
			{
				int i = y * n + x;
				tris.emplace_back(i, i + 1, i + n + 1);
				tris.emplace_back(i, i + n + 1, i + n);
			}
		}
	}

	void checkCollapseMap(const std::vector<int> &collapseMap)
	{
		for (size_t i = 1; i < collapseMap.size(); i++)
		{
			ASSERT_GE(collapseMap[i], 0);
			ASSERT_LT(collapseMap[i], int(i)) << "vertex " << i;
		}
	}

	std::multiset<std::tuple<float, float, float>> positionSet(const std::vector<glm::vec3> &positions)
	{
		std::multiset<std::tuple<float, float, float>> out;
		for (const auto &pos : positions)
		{
			out.emplace(std::round(pos.x * 1000) / 1000, std::round(pos.y * 1000) / 1000, std::round(pos.z * 1000) / 1000);
		}
		return out;
	}
}

TEST(LodTest, flatGrid)
{
	std::vector<glm::vec3> positions;
	std::vector<glm::ivec3> tris;
	grid(5, positions, tris);

	auto result = progressiveMesh(positions, tris);

	// The permutation is a permutation
	std::set<int> unique(result.permutation.begin(), result.permutation.end());
	ASSERT_EQ(unique.size(), positions.size());
	ASSERT_EQ(*unique.begin(), 0);
	ASSERT_EQ(*unique.rbegin(), int(positions.size()) - 1);

	checkCollapseMap(result.collapseMap);

	// The 9 inner vertices of a flat grid can go, the 16 border vertices are the silhouette
	ASSERT_EQ(result.minLod, 16);

	std::vector<glm::ivec3> remapped = tris;
	for (auto &tri : remapped)
	{
		tri = {result.permutation[tri.x], result.permutation[tri.y], result.permutation[tri.z]};
	}

	ASSERT_EQ(lodTriangles(remapped, result.collapseMap, 25).size(), tris.size());
	size_t previous = tris.size();
	for (int count = 24; count >= result.minLod; count--)
	{
		size_t current = lodTriangles(remapped, result.collapseMap, count).size();
		ASSERT_LE(current, previous);
		previous = current;
	}
	// The border still makes a shape at the minimum LOD
	ASSERT_GT(lodTriangles(remapped, result.collapseMap, result.minLod).size(), 0);
}

TEST(LodTest, skeletalModel)
{
	auto medic = load("models/mdm/medic.mdm", "models/mdm/medic.mdx");
	auto &mesh = medic->meshes[0];
	const size_t numTris = mesh.tris.size();

	std::vector<glm::vec3> before;
	for (const auto &vert : mesh.vertices)
	{
		before.push_back(posedVertex(*medic, mesh, vert, 0));
	}

	medic->currentFrame = 0;
	ASSERT_TRUE(calculateLod(*medic));

	// Same vertices in a new order, same triangles
	std::vector<glm::vec3> after;
	for (const auto &vert : mesh.vertices)
	{
		after.push_back(posedVertex(*medic, mesh, vert, 0));
	}
	ASSERT_EQ(positionSet(before), positionSet(after));
	ASSERT_EQ(mesh.tris.size(), numTris);
	ASSERT_EQ(mesh.collapseMap.size(), mesh.vertices.size());
	checkCollapseMap(mesh.collapseMap);
	ASSERT_GT(mesh.minLod, 0);
	ASSERT_LT(mesh.minLod, int(mesh.vertices.size()));

	auto reduced = lodTriangles(mesh.tris, mesh.collapseMap, mesh.minLod);
	ASSERT_GT(reduced.size(), 0);
	ASSERT_LT(reduced.size(), numTris);
}

TEST(LodTest, morphModel)
{
	mt::io::MTPath data("data");
	std::unique_ptr<Model> loaded(model::io::loadModel(data.loadFile("models/players/grunt/head.md3")));
	auto *head = dynamic_cast<morph::MorphModel *>(loaded.get());
	ASSERT_TRUE(head);

	auto texCoords = head->meshes[0].texCoords;
	ASSERT_TRUE(calculateLod(*head));
	ASSERT_NO_THROW(head->validate());
	checkCollapseMap(head->meshes[0].collapseMap);
	ASSERT_EQ(head->meshes[0].texCoords.size(), texCoords.size());
}

TEST(LodTest, invalidTriangles)
{
	ASSERT_THROW(progressiveMesh({{0, 0, 0}}, {{0, 1, 2}}), mt_exception);
	// Degenerate triangles are ignored
	auto result = progressiveMesh({{0, 0, 0}, {1, 0, 0}}, {{0, 0, 1}});
	ASSERT_EQ(result.permutation.size(), 2);
}

TEST(EditingTest, bakeBindPose)
{
	auto medic = load("models/mdm/medic.mdm", "models/mdm/medic.mdx");
	ASSERT_FALSE(medic->hasBindPose);

	auto &mesh = medic->meshes[0];
	glm::vec3 framePosition = posedVertex(*medic, mesh, mesh.vertices[10], 100);

	bakeBindPose(*medic, 100);
	ASSERT_TRUE(medic->hasBindPose);
	expectVec(mesh.vertices[10].pos, framePosition);
	expectVec(posedVertex(*medic, mesh, mesh.vertices[10], FRAME_BIND_POSE), framePosition);
	expectVec(medic->joints[5].location, medic->joints[5].frames[100].location);
	ASSERT_NEAR(glm::length(mesh.vertices[10].bindNormal), 1.0f, 1e-4f);

	// The other frames still work
	PoseOptions options;
	options.frame = options.nextFrame = 200;
	ASSERT_EQ(poseModel(*medic, options).meshes.size(), 4);

	ASSERT_THROW(bakeBindPose(*medic, 99999), mt_exception);
}

TEST(EditingTest, bakeTagsRemapsEverything)
{
	SkeletalModel model;
	model.hasBindPose = true;
	model.torsoParent = 2;

	Joint root;
	root.name = "root";
	root.parentId = -1;
	root.location = {0, 0, 10};
	root.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1});
	model.joints.push_back(root);

	Joint tag;
	tag.name = "tag_weapon";
	tag.parentId = 0;
	tag.asTag = true;
	tag.location = {0, 5, 10}; // 5 units along the root's (turned) x axis
	tag.rotation = root.rotation;
	model.joints.push_back(tag);

	Joint arm;
	arm.name = "arm";
	arm.parentId = 0;
	model.joints.push_back(arm);

	Mesh mesh;
	SkeletalVertex vert;
	vert.weightCount = 1;
	mesh.vertices.push_back(vert);
	mesh.weights.push_back({2, 1.0f, {1, 0, 0}});
	model.meshes.push_back(mesh);

	Hitbox hitbox(2);
	hitbox.parentJoint = 2;
	model.hitboxes.push_back(hitbox);

	ASSERT_TRUE(canBeTag(model, 1));
	ASSERT_FALSE(canBeTag(model, 0)); // has children
	ASSERT_FALSE(canBeTag(model, 2)); // has a weight and a hitbox

	ASSERT_EQ(bakeTags(model), 1);
	ASSERT_EQ(model.joints.size(), 2);
	ASSERT_EQ(model.joints[1].name, "arm");
	ASSERT_EQ(model.meshes[0].weights[0].jointId, 1);
	ASSERT_EQ(model.hitboxes[0].parentJoint, 1);
	ASSERT_EQ(model.torsoParent, 1);

	ASSERT_EQ(model.tags.size(), 1);
	ASSERT_EQ(model.tags[0].name, "tag_weapon");
	ASSERT_EQ(model.tags[0].parentBone, 0);
	expectVec(model.tags[0].location, {5, 0, 0});
	ASSERT_NEAR(std::abs(glm::dot(model.tags[0].rotation, glm::quat(1, 0, 0, 0))), 1.0f, 1e-5f);

	// Nothing left to bake
	ASSERT_EQ(bakeTags(model), 0);
}

TEST(EditingTest, bakeTagsOnMs3dKeepsTagPositions)
{
	auto model = load("models/ms3d/test.ms3d");
	model->flipBoneTags = false;

	PoseOptions bind;
	bind.bindPose = true;
	auto before = poseModel(*model, bind).tags;
	ASSERT_EQ(before.size(), 17);

	ASSERT_EQ(bakeTags(*model), 17);
	ASSERT_EQ(model->joints.size(), 53);
	ASSERT_TRUE(et::checkArmature(*model).empty());

	auto after = poseModel(*model, bind).tags;
	ASSERT_EQ(after.size(), 17);
	for (const auto &tag : before)
	{
		auto found = std::find_if(after.begin(), after.end(), [&](const PosedTag &t) { return t.name == tag.name; });
		ASSERT_NE(found, after.end()) << tag.name;
		expectVec(found->point.location, tag.point.location);
	}
}

TEST(EditingTest, reorderJoints)
{
	auto model = load("models/ms3d/test.ms3d");

	auto &mesh = model->meshes[0];
	glm::vec3 vertex = posedVertex(*model, mesh, mesh.vertices[20], 10);
	std::string torso = model->joints[model->torsoParent].name;

	// Move the tag bones to the front of the list: their parents would come after them
	std::vector<int> bad(model->joints.size());
	std::iota(bad.rbegin(), bad.rend(), 0);
	ASSERT_THROW(reorderJoints(*model, bad), mt_exception);
	ASSERT_THROW(reorderJoints(*model, {0, 1}), mt_exception);

	// The MS3D file is not in the default order (index 45 is a finger bone, not Bip01 L Thigh)
	ASSERT_NE(model->joints[45].name, "Bip01 L Thigh");

	int moved = et::reorganizeArmature(*model);
	ASSERT_GT(moved, 0);
	for (size_t i = 0; i < et::defaultArmature().size(); i++)
	{
		ASSERT_EQ(model->joints[i].name, et::defaultArmature()[i].name);
	}

	// Nothing moves in the model
	expectVec(posedVertex(*model, mesh, mesh.vertices[20], 10), vertex);
	ASSERT_EQ(model->joints[model->torsoParent].name, torso);
	ASSERT_TRUE(et::checkArmature(*model).empty());

	// Running it again changes nothing
	ASSERT_EQ(et::reorganizeArmature(*model), 0);
}

TEST(EditingTest, scaleBounds)
{
	auto medic = load("models/mdm/medic.mdm", "models/mdm/medic.mdx");
	Bounds original = medic->bounds[3];

	scaleBounds(*medic, 2.0f, BoundsAxes::Z);
	expectVec(medic->bounds[3].min, original.min * glm::vec3{1, 1, 2});
	scaleBounds(*medic, 0.5f, BoundsAxes::All);
	expectVec(medic->bounds[3].max, original.max * glm::vec3{0.5f, 0.5f, 1.0f});

	mt::io::MTPath data("data");
	std::unique_ptr<Model> head(model::io::loadModel(data.loadFile("models/players/grunt/head.md3")));
	auto *morph = dynamic_cast<morph::MorphModel *>(head.get());
	Bounds bounds = morph->frames[0].bounds;
	scaleBounds(*morph, 3.0f, BoundsAxes::XY);
	expectVec(morph->frames[0].bounds.max, bounds.max * glm::vec3{3, 3, 1});
	expectVec(morph->frames[0].localOrigin, (morph->frames[0].bounds.min + morph->frames[0].bounds.max) * 0.5f);
}
