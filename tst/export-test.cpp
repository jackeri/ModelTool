#include "gtest/gtest.h"
#include "model_mdm.h"
#include "model_mdx.h"
#include "model_md3.h"
#include "model_md5.h"
#include "model_scripts.h"
#include "model_stl.h"
#include "model_export.h"
#include "posed_model.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "morph_model.h"
#include "mt_path.h"

#include <filesystem>

using namespace mt;
using namespace mt::model;
using namespace mt::model::skeletal;

namespace {
	Ref<mt::io::MTFile> memoryFile(const std::string &name, const std::vector<byte> &data)
	{
		auto file = make_ref<mt::io::MTFile>();
		file->data = make_ref<std::vector<byte>>(data);
		file->len = data.size();
		file->data->push_back(0);
		file->name = name;
		file->ext = model::io::normalizeExtension(name);
		return file;
	}

	Ref<mt::io::MTFile> memoryFile(const std::string &name, const std::string &text)
	{
		return memoryFile(name, std::vector<byte>(text.begin(), text.end()));
	}

	template<typename T = Model>
	std::unique_ptr<T> loadModel(const Ref<mt::io::MTFile> &file)
	{
		std::unique_ptr<Model> loaded(model::io::loadModel(file));
		auto *typed = dynamic_cast<T *>(loaded.get());
		if (typed)
		{
			loaded.release();
		}
		return std::unique_ptr<T>(typed);
	}

	std::unique_ptr<SkeletalModel> loadMedic()
	{
		mt::io::MTPath data("data");
		auto model = loadModel<SkeletalModel>(data.loadFile("models/mdm/medic.mdm"));
		model::io::loadAnimation(model.get(), data.loadFile("models/mdm/medic.mdx"));
		model::io::loadAnimation(model.get(), data.loadFile("models/mdm/medic.aninc"));
		return model;
	}

	std::unique_ptr<SkeletalModel> loadBob()
	{
		mt::io::MTPath data("data");
		auto model = loadModel<SkeletalModel>(data.loadFile("models/md5/bob_lamp_update_export.md5mesh"));
		model::io::loadAnimation(model.get(), data.loadFile("models/md5/bob_lamp_update_export.md5anim"));
		return model;
	}

	PosedModel pose(const Model &model, int frame)
	{
		PoseOptions options;
		options.bindPose = frame == FRAME_BIND_POSE;
		options.frame = options.nextFrame = std::max(frame, 0);
		return poseModel(model, options);
	}

	/**
	 * Largest distance between the posed vertices of two models (meshes and vertices in the same order)
	 */
	float maxVertexDistance(const PosedModel &a, const PosedModel &b, const glm::mat4 &transform = glm::mat4{1.0f})
	{
		EXPECT_EQ(a.meshes.size(), b.meshes.size());
		float worst = 0;
		for (size_t m = 0; m < std::min(a.meshes.size(), b.meshes.size()); m++)
		{
			EXPECT_EQ(a.meshes[m].positions.size(), b.meshes[m].positions.size());
			for (size_t v = 0; v < std::min(a.meshes[m].positions.size(), b.meshes[m].positions.size()); v++)
			{
				glm::vec3 expected = glm::vec3(transform * glm::vec4(a.meshes[m].positions[v], 1.0f));
				worst = std::max(worst, glm::distance(expected, b.meshes[m].positions[v]));
			}
		}
		return worst;
	}

	const PosedTag *findTag(const PosedModel &posed, const std::string &name)
	{
		auto found = std::find_if(posed.tags.begin(), posed.tags.end(), [&](const PosedTag &tag) { return tag.name == name; });
		return found == posed.tags.end() ? nullptr : &*found;
	}

	/*! Folder for files written by the tests, removed afterwards */
	struct TempFolder {
		std::filesystem::path path = std::filesystem::path("tmp") / "export-test";

		TempFolder()
		{
			std::filesystem::create_directories(path);
		}

		~TempFolder()
		{
			std::filesystem::remove_all(path);
		}

		[[nodiscard]] std::string file(const std::string &name) const
		{
			return (path / name).string();
		}
	};
}

TEST(ExportTest, mdmAndMdxRoundTrip)
{
	auto medic = loadMedic();

	auto loaded = loadModel<SkeletalModel>(memoryFile("medic.mdm", encodeMDM(*medic)));
	ASSERT_TRUE(loaded);
	model::io::loadAnimation(loaded.get(), memoryFile("medic.mdx", encodeMDX(*medic)));

	ASSERT_EQ(loaded->name, medic->name);
	ASSERT_EQ(loaded->numFrames(), medic->numFrames());
	ASSERT_EQ(loaded->joints.size(), medic->joints.size());
	ASSERT_EQ(loaded->torsoParent, medic->torsoParent);
	for (size_t i = 0; i < medic->joints.size(); i++)
	{
		ASSERT_EQ(loaded->joints[i].name, medic->joints[i].name);
		ASSERT_EQ(loaded->joints[i].parentId, medic->joints[i].parentId);
	}

	for (size_t m = 0; m < medic->meshes.size(); m++)
	{
		ASSERT_EQ(loaded->meshes[m].name, medic->meshes[m].name);
		ASSERT_EQ(loaded->meshes[m].tris, medic->meshes[m].tris);
		ASSERT_EQ(loaded->meshes[m].collapseMap, medic->meshes[m].collapseMap);
		ASSERT_EQ(loaded->meshes[m].minLod, medic->meshes[m].minLod);
		// MDM normals are written back as they were read
		ASSERT_EQ(loaded->meshes[m].vertices[3].normal, medic->meshes[m].vertices[3].normal);
	}

	// The bone angles and directions are stored as 16 bit angles, so the result is close but not exact
	for (int frame : {0, 700, 2000, 4886})
	{
		auto a = pose(*medic, frame);
		auto b = pose(*loaded, frame);
		EXPECT_LT(maxVertexDistance(a, b), 0.05f) << "frame " << frame;

		ASSERT_EQ(b.tags.size(), a.tags.size());
		for (const auto &tag : a.tags)
		{
			auto *other = findTag(b, tag.name);
			ASSERT_TRUE(other) << tag.name;
			EXPECT_LT(glm::distance(tag.point.location, other->point.location), 0.05f) << tag.name;
			EXPECT_GT(std::abs(glm::dot(tag.point.rotation, other->point.rotation)), 0.9999f) << tag.name;
		}
	}

	// An MDM without its MDX writes the same file again (the bone references come from the file)
	mt::io::MTPath data("data");
	auto bare = loadModel<SkeletalModel>(data.loadFile("models/mdm/medic.mdm"));
	auto rewritten = encodeMDM(*bare);
	auto bareAgain = loadModel<SkeletalModel>(memoryFile("again.mdm", rewritten));
	ASSERT_EQ(bareAgain->meshes[0].boneReferences, bare->meshes[0].boneReferences);
	ASSERT_EQ(bareAgain->tags[0].boneReferences, bare->tags[0].boneReferences);
	// Tag axes go through a quaternion, so they come back with float rounding
	for (size_t t = 0; t < bare->tags.size(); t++)
	{
		EXPECT_GT(std::abs(glm::dot(bareAgain->tags[t].rotation, bare->tags[t].rotation)), 0.999999f);
		EXPECT_EQ(bareAgain->tags[t].location, bare->tags[t].location);
	}
	ASSERT_EQ(bareAgain->meshes[0].weights.size(), bare->meshes[0].weights.size());
	ASSERT_EQ(bareAgain->meshes[0].weights[7].pos, bare->meshes[0].weights[7].pos);
}

TEST(ExportTest, mdsRoundTrip)
{
	auto medic = loadMedic();
	auto loaded = loadModel<SkeletalModel>(memoryFile("medic.mds", encodeMDS(*medic)));
	ASSERT_TRUE(loaded);

	ASSERT_EQ(loaded->numFrames(), medic->numFrames());
	// The 17 MDM tags become tag bones
	ASSERT_EQ(loaded->joints.size(), medic->joints.size() + 17);
	ASSERT_EQ(loaded->tags.size(), 17);

	for (int frame : {0, 2000})
	{
		auto a = pose(*medic, frame);
		auto b = pose(*loaded, frame);
		EXPECT_LT(maxVertexDistance(a, b), 0.05f) << "frame " << frame;

		for (const auto &tag : a.tags)
		{
			auto *other = findTag(b, tag.name);
			ASSERT_TRUE(other) << tag.name;
			EXPECT_LT(glm::distance(tag.point.location, other->point.location), 0.05f) << tag.name;
		}
	}

	// MDS to MDS keeps the tags as they are (no new bones)
	auto again = loadModel<SkeletalModel>(memoryFile("again.mds", encodeMDS(*loaded)));
	ASSERT_EQ(again->joints.size(), loaded->joints.size());
	ASSERT_EQ(again->tags.size(), loaded->tags.size());
}

TEST(ExportTest, md5RoundTrip)
{
	auto bob = loadBob();

	auto loaded = loadModel<SkeletalModel>(memoryFile("bob.md5mesh", encodeMD5Mesh(*bob)));
	ASSERT_TRUE(loaded);
	model::io::loadAnimation(loaded.get(), memoryFile("bob.md5anim", encodeMD5Anim(*bob)));

	ASSERT_EQ(loaded->numFrames(), bob->numFrames());
	ASSERT_EQ(loaded->meshes[0].materialName, "bob_body");
	EXPECT_LT(maxVertexDistance(pose(*bob, FRAME_BIND_POSE), pose(*loaded, FRAME_BIND_POSE)), 1e-3f);
	for (int frame : {0, 60, 139})
	{
		EXPECT_LT(maxVertexDistance(pose(*bob, frame), pose(*loaded, frame)), 1e-3f) << "frame " << frame;
	}
}

TEST(ExportTest, md5FromModelWithoutBindPose)
{
	// The medic has no bind pose, the first frame becomes the MD5 bind pose
	auto medic = loadMedic();
	medic->firstExportFrame = 100;
	medic->lastExportFrame = 109;

	auto loaded = loadModel<SkeletalModel>(memoryFile("medic.md5mesh", encodeMD5Mesh(*medic)));
	model::io::loadAnimation(loaded.get(), memoryFile("medic.md5anim", encodeMD5Anim(*medic)));

	ASSERT_EQ(loaded->numFrames(), 10);
	EXPECT_LT(maxVertexDistance(pose(*medic, 100), pose(*loaded, FRAME_BIND_POSE)), 1e-3f);
	EXPECT_LT(maxVertexDistance(pose(*medic, 105), pose(*loaded, 5)), 1e-3f);
}

TEST(ExportTest, md5BaseFrame)
{
	// The second joint is not animated: its values come from the baseframe (they used to become 0)
	mt::io::MTPath data("data");
	auto bob = loadModel<SkeletalModel>(data.loadFile("models/md5/bob_lamp_update_export.md5mesh"));
	std::string anim
		= "MD5Version 10\nnumFrames 1\nnumJoints " + std::to_string(bob->joints.size()) + "\nframeRate 24\nnumAnimatedComponents 0\n";
	anim += "hierarchy {\n";
	for (const auto &joint : bob->joints)
	{
		anim += "\"" + joint.name + "\" " + std::to_string(joint.parentId) + " 0 0\n";
	}
	anim += "}\nbounds {\n( 0 0 0 ) ( 1 1 1 )\n}\nbaseframe {\n";
	for (size_t i = 0; i < bob->joints.size(); i++)
	{
		anim += i == 0 ? "( 1 2 3 ) ( 0 0 0 )\n" : "( 0 0 1 ) ( 0 0 0 )\n";
	}
	anim += "}\nframe 0 {\n}\n";

	model::io::loadAnimation(bob.get(), memoryFile("still.md5anim", anim));
	const Point &root = bob->joints[0].frames[0];
	EXPECT_NEAR(root.location.x, 1, 1e-5f);
	EXPECT_NEAR(root.location.z, 3, 1e-5f);
	const Point &child = bob->joints[1].frames[0];
	EXPECT_NEAR(child.location.z, 4, 1e-5f);
}

TEST(ExportTest, md3RoundTrip)
{
	mt::io::MTPath data("data");
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/grunt/head.md3"));
	auto loaded = loadModel<morph::MorphModel>(memoryFile("head.md3", encodeMD3(*head)));
	ASSERT_TRUE(loaded);

	const auto &a = head->meshes[0];
	const auto &b = loaded->meshes[0];
	ASSERT_EQ(b.name, a.name);
	ASSERT_EQ(b.tris, a.tris);
	ASSERT_EQ(b.texCoords, a.texCoords);
	for (size_t v = 0; v < a.numVertices(); v++)
	{
		ASSERT_EQ(b.frames[0][v].pos, a.frames[0][v].pos); // already on the 1/64 grid
		EXPECT_GT(glm::dot(b.frames[0][v].normal, a.frames[0][v].normal), 0.99f);
	}

	ASSERT_EQ(loaded->tags.size(), 1);
	EXPECT_EQ(loaded->tags[0].frames[0].location, head->tags[0].frames[0].location);
}

TEST(ExportTest, md3LevelsOfDetailAndTagsOnly)
{
	mt::io::MTPath data("data");
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/grunt/head.md3"));
	addMorphLod(*head, data.loadFile("models/players/grunt/head_1.md3"));

	TempFolder folder;
	writeMD3(*head, folder.file("head.md3"));
	ASSERT_TRUE(std::filesystem::exists(folder.file("head.md3")));
	ASSERT_TRUE(std::filesystem::exists(folder.file("head_1.md3")));

	mt::io::MTPath written(folder.path.string());
	auto lod = loadModel<morph::MorphModel>(written.loadFile("head_1.md3"));
	ASSERT_EQ(lod->meshes[0].name, "h_head_1");

	writeMD3Tags(*head, folder.file("tags.md3"));
	auto tags = loadModel<morph::MorphModel>(written.loadFile("tags.md3"));
	ASSERT_TRUE(tags->meshes.empty());
	ASSERT_EQ(tags->tags.size(), 1);
}

TEST(ExportTest, skeletalToMd3)
{
	auto bob = loadBob();
	auto loaded = loadModel<morph::MorphModel>(memoryFile("bob.md3", encodeMD3(*bob)));
	ASSERT_TRUE(loaded);
	ASSERT_EQ(loaded->numFrames(), bob->numFrames());

	// Positions are rounded to 1/64 units
	EXPECT_LT(maxVertexDistance(pose(*bob, 60), pose(*loaded, 60)), 1.0f / 64.0f);

	// Too large for MD3
	bob->scale = 100.0f;
	ASSERT_THROW(encodeMD3(*bob), mt_exception);
}

TEST(ExportTest, transformIsApplied)
{
	auto medic = loadMedic();
	auto original = pose(*medic, 300);

	medic->firstExportFrame = 295; // fewer frames to keep the test fast
	medic->lastExportFrame = 305;
	medic->scale = 2.0f;
	medic->transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1});
	medic->transform.location = {10, 0, 5};
	const glm::mat4 transform
		= glm::translate(medic->transform.location) * glm::toMat4(medic->transform.rotation) * glm::scale(glm::vec3(2.0f));

	auto loaded = loadModel<SkeletalModel>(memoryFile("medic.mdm", encodeMDM(*medic)));
	model::io::loadAnimation(loaded.get(), memoryFile("medic.mdx", encodeMDX(*medic)));
	EXPECT_LT(maxVertexDistance(original, pose(*loaded, 5), transform), 0.1f);

	auto md5 = loadModel<SkeletalModel>(memoryFile("medic.md5mesh", encodeMD5Mesh(*medic)));
	model::io::loadAnimation(md5.get(), memoryFile("medic.md5anim", encodeMD5Anim(*medic)));
	EXPECT_LT(maxVertexDistance(original, pose(*md5, 5), transform), 1e-3f);

	// The model itself is not changed
	ASSERT_FLOAT_EQ(medic->scale, 2.0f);
}

TEST(ExportTest, exportFrames)
{
	auto medic = loadMedic();
	medic->firstExportFrame = 32;
	medic->lastExportFrame = 63;

	auto loaded = loadModel<SkeletalModel>(memoryFile("medic.mdm", encodeMDM(*medic)));
	model::io::loadAnimation(loaded.get(), memoryFile("medic.mdx", encodeMDX(*medic)));
	model::io::loadAnimation(loaded.get(), memoryFile("medic.aninc", encodeAninc(*medic)));

	ASSERT_EQ(loaded->numFrames(), 32);
	EXPECT_LT(maxVertexDistance(pose(*medic, 40), pose(*loaded, 8)), 0.05f);

	// Only the sequences in the range are kept, moved to the new frame numbers
	for (const auto &animation : loaded->animations)
	{
		ASSERT_GE(animation.startFrame, 0);
		ASSERT_LT(animation.lastFrame(), 32);
	}
	ASSERT_FALSE(loaded->animations.empty());
	ASSERT_LT(loaded->animations.size(), medic->animations.size());
}

TEST(ExportTest, cropFramesClipsSequences)
{
	auto medic = loadMedic();
	medic->animations = {{"inside", 10, 5, 20, true}, {"across", 5, 10, 20}, {"outside", 50, 5, 20}};
	Animation reversed{"back", 30, 15, 20};
	reversed.reversed = true; // frames 30 down to 16
	medic->animations.push_back(reversed);

	cropFrames(*medic, 8, 20);
	ASSERT_EQ(medic->numFrames(), 13);
	ASSERT_EQ(medic->animations.size(), 3);
	ASSERT_EQ(medic->animations[0].startFrame, 2);
	ASSERT_EQ(medic->animations[0].numFrames, 5);
	ASSERT_EQ(medic->animations[1].startFrame, 0); // frames 8 - 14
	ASSERT_EQ(medic->animations[1].numFrames, 7);
	ASSERT_EQ(medic->animations[2].name, "back"); // frames 20 down to 16
	ASSERT_EQ(medic->animations[2].startFrame, 12);
	ASSERT_EQ(medic->animations[2].lastFrame(), 8);
}

TEST(ExportTest, aninc)
{
	auto medic = loadMedic();
	medic->animations[3].reversed = true;
	medic->animations[3].moveSpeed = 160;

	auto loaded = loadMedic();
	model::io::loadAnimation(loaded.get(), memoryFile("medic.aninc", encodeAninc(*medic)));
	ASSERT_EQ(loaded->animations.size(), medic->animations.size());
	for (size_t i = 0; i < medic->animations.size(); i++)
	{
		const auto &a = medic->animations[i];
		const auto &b = loaded->animations[i];
		ASSERT_EQ(b.name, a.name);
		ASSERT_EQ(b.startFrame, a.startFrame);
		ASSERT_EQ(b.numFrames, a.numFrames);
		ASSERT_EQ(b.loop, a.loop);
		ASSERT_EQ(b.fps, a.fps);
		ASSERT_EQ(b.moveSpeed, a.moveSpeed);
		ASSERT_EQ(b.reversed, a.reversed);
	}

	medic->animations[0].name = "has space";
	ASSERT_THROW(encodeAninc(*medic), mt_exception);
}

TEST(ExportTest, tagsFile)
{
	auto medic = loadMedic();
	auto target = loadMedic();
	target->tags.clear();

	loadTags(*target, memoryFile("medic.tag", encodeTags(*medic)));
	ASSERT_EQ(target->tags.size(), medic->tags.size());
	for (size_t i = 0; i < medic->tags.size(); i++)
	{
		ASSERT_EQ(target->tags[i].name, medic->tags[i].name);
		ASSERT_EQ(target->tags[i].parentBone, medic->tags[i].parentBone);
		EXPECT_LT(glm::distance(target->tags[i].location, medic->tags[i].location), 1e-5f);
		EXPECT_GT(std::abs(glm::dot(target->tags[i].rotation, medic->tags[i].rotation)), 0.99999f);
	}
}

TEST(ExportTest, stl)
{
	mt::io::MTPath data("data");
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/grunt/head.md3"));
	const size_t triangles = head->meshes[0].tris.size();

	auto binary = encodeSTL(*head, true);
	ASSERT_EQ(binary.size(), 84 + 50 * triangles);
	uint32_t count;
	std::memcpy(&count, binary.data() + 80, sizeof(count));
	ASSERT_EQ(count, triangles);

	// The facet normals point the same way as the model's normals (outwards)
	float agreement = 0;
	const auto &frame = head->meshes[0].frames[0];
	for (size_t t = 0; t < triangles; t++)
	{
		glm::vec3 normal;
		std::memcpy(&normal, binary.data() + 84 + 50 * t, sizeof(normal));
		const auto &tri = head->meshes[0].tris[t];
		agreement += glm::dot(normal, glm::normalize(frame[tri.x].normal + frame[tri.y].normal + frame[tri.z].normal));
	}
	EXPECT_GT(agreement / float(triangles), 0.8f);

	auto ascii = encodeSTL(*head, false);
	std::string text(ascii.begin(), ascii.end());
	size_t facets = 0;
	for (size_t pos = text.find("facet normal"); pos != std::string::npos; pos = text.find("facet normal", pos + 1))
	{
		facets++;
	}
	ASSERT_EQ(facets, triangles);
	ASSERT_EQ(text.rfind("endsolid", 0), std::string::npos);
	ASSERT_NE(text.find("endsolid"), std::string::npos);
}

TEST(ExportTest, registryWriters)
{
	auto medic = loadMedic();
	TempFolder folder;

	model::io::writeModel(*medic, folder.file("medic.mdm"));
	model::io::writeAnimation(*medic, folder.file("medic.mdx"));
	model::io::writeAnimation(*medic, folder.file("medic.aninc"));

	mt::io::MTPath written(folder.path.string());
	auto loaded = loadModel<SkeletalModel>(written.loadFile("medic.mdm"));
	model::io::loadAnimation(loaded.get(), written.loadFile("medic.mdx"));
	model::io::loadAnimation(loaded.get(), written.loadFile("medic.aninc"));
	ASSERT_EQ(loaded->numFrames(), medic->numFrames());
	ASSERT_EQ(loaded->animations.size(), medic->animations.size());

	auto writers = model::io::modelWriterExtensions();
	for (const char *ext : {".md3", ".md5mesh", ".mdm", ".mds", ".mdx", ".stl", ".tag"})
	{
		ASSERT_NE(std::find(writers.begin(), writers.end(), ext), writers.end()) << ext;
	}
}

TEST(ExportTest, errors)
{
	mt::io::MTPath data("data");

	// The IQM armature has three root bones, MDX stores one
	auto fixit = loadModel<SkeletalModel>(data.loadFile("models/iqm/mrfixit.iqm"));
	ASSERT_THROW(encodeMDX(*fixit), mt_exception);

	// Names longer than the file format allows
	auto medic = loadMedic();
	medic->meshes[0].name = std::string(80, 'x');
	ASSERT_THROW(encodeMDM(*medic), mt_exception);

	// Morph models are not skeletal
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/grunt/head.md3"));
	ASSERT_THROW(encodeMDM(*head), mt_exception);
	ASSERT_THROW(encodeMD5Mesh(*head), mt_exception);
}

TEST(ExportTest, generatedLodForMeshesWithoutCollapseMaps)
{
	auto bob = loadBob();
	ASSERT_TRUE(bob->meshes[0].collapseMap.empty());

	// MD5 has no collapse maps, the MDM writer calculates them
	auto loaded = loadModel<SkeletalModel>(memoryFile("bob.mdm", encodeMDM(*bob)));
	for (const auto &mesh : loaded->meshes)
	{
		ASSERT_EQ(mesh.collapseMap.size(), mesh.vertices.size());
		for (size_t i = 1; i < mesh.collapseMap.size(); i++)
		{
			ASSERT_LT(mesh.collapseMap[i], int(i));
		}
	}
	// The original model is not changed
	ASSERT_TRUE(bob->meshes[0].collapseMap.empty());
}
