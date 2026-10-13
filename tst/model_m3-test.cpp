#include "gtest/gtest.h"
#include "model_iqm.h"
#include "model_ms3d.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "et_armature.h"
#include "normals.h"
#include "mt_path.h"

using namespace mt;
using namespace mt::model;
using namespace mt::model::skeletal;

namespace {
	std::unique_ptr<SkeletalModel> loadSkeletal(const std::string &path)
	{
		mt::io::MTPath data("data");
		std::unique_ptr<Model> loaded(model::io::loadModel(data.loadFile(path)));
		return std::unique_ptr<SkeletalModel>(dynamic_cast<SkeletalModel *>(loaded.release()));
	}

	// Mean distance between the positions each weight of a vertex gives, 0 when the weights agree
	double weightSpread(const SkeletalModel &model, int frame)
	{
		double total = 0;
		int count = 0;
		for (const auto &mesh : model.meshes)
		{
			for (const auto &vert : mesh.vertices)
			{
				glm::vec3 pos = posedVertex(model, mesh, vert, frame);
				for (int i = 0; i < vert.weightCount; i++)
				{
					const auto &weight = mesh.weights[vert.startWeight + i];
					const auto &joint = model.joints[weight.jointId];
					const Point &pose = frame == FRAME_BIND_POSE ? static_cast<const Point &>(joint) : joint.frames[frame];
					total += glm::distance(pose.location + pose.rotation * weight.pos, pos);
					count++;
				}
			}
		}
		return count ? total / count : 0;
	}

	// Mean relative change of the triangle edge lengths compared to the bind pose
	double edgeStretch(const SkeletalModel &model, int frame)
	{
		double total = 0;
		int count = 0;
		for (const auto &mesh : model.meshes)
		{
			for (const auto &tri : mesh.tris)
			{
				for (int i = 0; i < 3; i++)
				{
					const auto &a = mesh.vertices[tri[i]];
					const auto &b = mesh.vertices[tri[(i + 1) % 3]];
					float rest = glm::distance(a.pos, b.pos);
					if (rest < 1e-3f)
					{
						continue;
					}
					float posed = glm::distance(posedVertex(model, mesh, a, frame), posedVertex(model, mesh, b, frame));
					total += std::abs(posed - rest) / rest;
					count++;
				}
			}
		}
		return total / count;
	}

	double meanDot(const std::vector<glm::vec3> &a, const std::vector<glm::vec3> &b)
	{
		double total = 0;
		for (size_t i = 0; i < a.size(); i++)
		{
			total += glm::dot(a[i], b[i]);
		}
		return total / double(a.size());
	}

	std::vector<glm::vec3> bindNormals(const SkeletalModel &model)
	{
		std::vector<glm::vec3> normals;
		for (const auto &mesh : model.meshes)
		{
			for (const auto &vert : mesh.vertices)
			{
				normals.push_back(vert.bindNormal);
			}
		}
		return normals;
	}
}

TEST(ModelIQMTest, loads)
{
	auto model = loadSkeletal("models/iqm/mrfixit.iqm");
	ASSERT_TRUE(model);

	ASSERT_EQ(model->name, "mrfixit");
	ASSERT_TRUE(model->hasBindPose);
	ASSERT_EQ(model->joints.size(), 75);
	ASSERT_EQ(model->numFrames(), 101);
	ASSERT_EQ(model->bounds.size(), 101);

	ASSERT_EQ(model->meshes.size(), 2);
	ASSERT_EQ(model->meshes[0].name, "Body");
	ASSERT_EQ(model->meshes[0].materialName, "Body.tga");
	ASSERT_EQ(model->meshes[0].vertices.size(), 1476);
	ASSERT_EQ(model->meshes[1].name, "Head");

	ASSERT_EQ(model->animations.size(), 1);
	ASSERT_EQ(model->animations[0].name, "idle");
	ASSERT_EQ(model->animations[0].numFrames, 101);
	ASSERT_EQ(model->animations[0].fps, 24);

	// Weights add up to 1 and give back the bind pose
	for (const auto &mesh : model->meshes)
	{
		for (const auto &vert : mesh.vertices)
		{
			ASSERT_GT(vert.weightCount, 0);
			float total = 0;
			for (int i = 0; i < vert.weightCount; i++)
			{
				total += mesh.weights[vert.startWeight + i].bias;
			}
			ASSERT_NEAR(total, 1.0f, 1e-5f);

			glm::vec3 pos = posedVertex(*model, mesh, vert, FRAME_BIND_POSE);
			ASSERT_NEAR(glm::distance(pos, vert.pos), 0.0f, 1e-4f);
		}
	}

	// The animated weights agree on the vertex positions
	ASSERT_LT(weightSpread(*model, 50), 0.05);
}

TEST(ModelIQMTest, normalsUseTheIdTechWinding)
{
	auto model = loadSkeletal("models/iqm/mrfixit.iqm");
	auto stored = bindNormals(*model);

	calculateNormals(*model);
	ASSERT_GT(meanDot(stored, bindNormals(*model)), 0.85);
}

TEST(ModelIQMTest, errors)
{
	mt::io::MTPath data("data");

	auto file = data.loadFile("models/iqm/mrfixit.iqm");
	file->len /= 2;
	ASSERT_THROW(loadIQM(file), mt_exception);

	file = data.loadFile("models/iqm/mrfixit.iqm");
	(*file->data)[0] = 'X';
	ASSERT_THROW(loadIQM(file), mt_exception);

	ASSERT_THROW(loadIQM(data.loadFile("models/ms3d/test.ms3d")), mt_exception);
}

TEST(ModelMS3DTest, loads)
{
	auto model = loadSkeletal("models/ms3d/test.ms3d");
	ASSERT_TRUE(model);

	ASSERT_EQ(model->name, "test");
	ASSERT_TRUE(model->hasBindPose);
	ASSERT_TRUE(model->flipBoneTags);
	ASSERT_EQ(model->joints.size(), 70);
	ASSERT_EQ(model->numFrames(), 96);
	ASSERT_EQ(model->bounds.size(), 96);

	// Generated test model (tools/fixtures): the mannequin on the default armature with the tags as extra bones
	ASSERT_EQ(model->meshes.size(), 4);
	ASSERT_EQ(model->meshes[0].name, "u_body");
	ASSERT_EQ(model->meshes[0].vertices.size(), 384);
	ASSERT_EQ(model->meshes[0].minLod, 384);
	ASSERT_EQ(model->meshes[2].name, "l_legs");
	ASSERT_EQ(model->meshes[2].vertices.size(), 248);
	ASSERT_TRUE(et::checkArmature(*model).empty());
	ASSERT_EQ(std::count_if(model->joints.begin(), model->joints.end(), [](const Joint &joint) { return joint.asTag; }), 17);
	ASSERT_EQ(model->joints[model->torsoParent].name, "Bip01 Spine");
	auto spine2 = std::find_if(model->joints.begin(), model->joints.end(), [](const Joint &joint) { return joint.name == "Bip01 Spine2"; });
	ASSERT_FLOAT_EQ(spine2->torsoWeight, 0.55f);

	ASSERT_EQ(model->animations.size(), 1);
	ASSERT_EQ(model->animations[0].name, "test");
	ASSERT_EQ(model->animations[0].numFrames, 96);
	ASSERT_EQ(model->animations[0].fps, 20);

	// Y-up to Z-up
	glm::vec3 up = model->transform.rotation * glm::vec3{0, 1, 0};
	ASSERT_NEAR(up.z, 1.0f, 1e-5f);

	// The file has no normals, they are calculated
	for (const auto &vert : model->meshes[0].vertices)
	{
		ASSERT_NEAR(glm::length(vert.bindNormal), 1.0f, 1e-4f);
	}

	// Rotations are applied around X, then Y, then Z: with the right order the mesh keeps its shape
	ASSERT_LT(edgeStretch(*model, 48), 0.05);
}

TEST(ModelMS3DTest, appendAnimation)
{
	auto model = loadSkeletal("models/ms3d/test.ms3d");
	mt::io::MTPath data("data");

	auto frameZero = model->joints[5].frames[0];
	model::io::loadAnimation(model.get(), data.loadFile("models/ms3d/test.ms3d"));

	ASSERT_EQ(model->numFrames(), 192);
	ASSERT_EQ(model->bounds.size(), 192);
	ASSERT_EQ(model->animations.size(), 2);
	ASSERT_EQ(model->animations[1].startFrame, 96);
	ASSERT_EQ(model->joints[5].frames[96].location, frameZero.location);

	// The joints need to match
	auto mannequin = loadSkeletal("models/mdm/mannequin.mdx");
	ASSERT_THROW(model::io::loadAnimation(mannequin.get(), data.loadFile("models/ms3d/test.ms3d")), mt_exception);
	ASSERT_EQ(mannequin->numFrames(), 116);
}

TEST(ModelMS3DTest, errors)
{
	mt::io::MTPath data("data");

	auto file = data.loadFile("models/ms3d/test.ms3d");
	file->len = 5000;
	ASSERT_THROW(loadMS3D(file), mt_exception);

	file = data.loadFile("models/ms3d/test.ms3d");
	(*file->data)[0] = 'X';
	ASSERT_THROW(loadMS3D(file), mt_exception);
}

TEST(SkeletalToolsTest, optimizeVertices)
{
	SkeletalModel model;
	model.hasBindPose = true;

	Joint root;
	root.name = "root";
	root.parentId = -1;
	model.joints.push_back(root);

	Mesh mesh;
	auto addVertex = [&](glm::vec3 pos, glm::vec2 tex) {
		SkeletalVertex vert;
		vert.pos = pos;
		vert.tex = tex;
		vert.bindNormal = {0, 0, 1};
		vert.startWeight = int(mesh.weights.size());
		vert.weightCount = 1;
		mesh.weights.push_back({0, 1.0f, pos});
		mesh.vertices.push_back(vert);
	};

	addVertex({0, 0, 0}, {0, 0});
	addVertex({1, 0, 0}, {1, 0});
	addVertex({0, 1, 0}, {0, 1});
	addVertex({1, 0, 0}, {1, 0});	 // duplicate of 1
	addVertex({0, 1, 0}, {0.5f, 1}); // same position as 2, different texture coordinates
	mesh.tris = {{0, 1, 2}, {0, 3, 4}};
	mesh.collapseMap = {0, 0, 0, 0, 0};
	mesh.minLod = 5;
	model.meshes.push_back(mesh);

	ASSERT_EQ(optimizeVertices(model), 1);

	const auto &out = model.meshes[0];
	ASSERT_EQ(out.vertices.size(), 4);
	ASSERT_EQ(out.weights.size(), 4);
	ASSERT_EQ(out.tris[0], glm::ivec3(0, 1, 2));
	ASSERT_EQ(out.tris[1], glm::ivec3(0, 1, 3));
	ASSERT_TRUE(out.collapseMap.empty());
	ASSERT_EQ(out.minLod, 4);

	for (const auto &vert : out.vertices)
	{
		ASSERT_EQ(posedVertex(model, out, vert, FRAME_BIND_POSE), vert.pos);
	}

	// Nothing more to merge
	ASSERT_EQ(optimizeVertices(model), 0);
}

TEST(SkeletalToolsTest, calculateBounds)
{
	mt::io::MTPath data("data");
	std::unique_ptr<Model> mesh(model::io::loadModel(data.loadFile("models/mdm/mannequin.mdm")));
	model::io::loadAnimation(mesh.get(), data.loadFile("models/mdm/mannequin.mdx"));
	auto *mannequin = dynamic_cast<SkeletalModel *>(mesh.get());

	auto stored = mannequin->bounds;
	calculateBounds(*mannequin);
	ASSERT_EQ(mannequin->bounds.size(), stored.size());

	// The calculated bounds are tight, the stored ones contain them
	for (int frame : {0, 100})
	{
		ASSERT_TRUE(glm::all(glm::greaterThanEqual(mannequin->bounds[frame].min, stored[frame].min - 0.5f)));
		ASSERT_TRUE(glm::all(glm::lessThanEqual(mannequin->bounds[frame].max, stored[frame].max + 0.5f)));
	}

	// Only the requested frames are recalculated, with the scale applied
	Bounds unscaled = mannequin->bounds[0];
	mannequin->bounds[1] = {};
	calculateBounds(*mannequin, 2.0f, 0, 1);
	ASSERT_EQ(mannequin->bounds[0].min, unscaled.min * 2.0f);
	ASSERT_EQ(mannequin->bounds[0].max, unscaled.max * 2.0f);
	ASSERT_EQ(mannequin->bounds[1].max, glm::vec3(0));
}

TEST(EtArmatureTest, defaultArmature)
{
	auto model = loadSkeletal("models/mdm/mannequin.mdx");

	// The mannequin uses the default armature
	ASSERT_EQ(et::defaultArmature().size(), 53);
	ASSERT_TRUE(et::checkArmature(*model).empty());

	for (auto &joint : model->joints)
	{
		joint.torsoWeight = 0;
	}
	model->torsoParent = -1;
	et::applyDefaultJointInformation(*model);
	ASSERT_EQ(model->torsoParent, 1);
	ASSERT_FLOAT_EQ(model->joints[4].torsoWeight, 0.85f); // Bip01 Spine3

	// Renamed bone and a changed parent are reported
	model->joints[6].name = "head";
	model->joints[45].parentId = 0;
	auto issues = et::checkArmature(*model);
	ASSERT_EQ(issues.size(), 2);
	ASSERT_NE(issues[0].find("Bip01 Head"), std::string::npos);
	ASSERT_NE(issues[1].find("Bip01 L Thigh"), std::string::npos);
}

TEST(EtArmatureTest, markTagBones)
{
	SkeletalModel model;
	for (auto [name, parent] :
		 std::vector<std::pair<std::string, int>>{{"root", -1}, {"tag_weapon", 0}, {"TAG_head", 0}, {"tag_parent", 0}, {"child", 3}})
	{
		Joint joint;
		joint.name = name;
		joint.parentId = parent;
		model.joints.push_back(joint);
	}

	et::markTagBones(model);
	ASSERT_FALSE(model.joints[0].asTag);
	ASSERT_TRUE(model.joints[1].asTag);
	ASSERT_TRUE(model.joints[2].asTag);
	ASSERT_FALSE(model.joints[3].asTag); // has a child bone
	ASSERT_FALSE(model.joints[4].asTag); // not named tag_
}
