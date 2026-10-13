#include "gtest/gtest.h"
#include "et_armature.h"
#include "model_ms3d.h"
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

	std::unique_ptr<SkeletalModel> loadMannequin()
	{
		mt::io::MTPath data("data");
		auto model = loadModel<SkeletalModel>(data.loadFile("models/mdm/mannequin.mdm"));
		model::io::loadAnimation(model.get(), data.loadFile("models/mdm/mannequin.mdx"));
		model::io::loadAnimation(model.get(), data.loadFile("models/mdm/mannequin.aninc"));
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
	auto mannequin = loadMannequin();

	auto loaded = loadModel<SkeletalModel>(memoryFile("mannequin.mdm", encodeMDM(*mannequin)));
	ASSERT_TRUE(loaded);
	model::io::loadAnimation(loaded.get(), memoryFile("mannequin.mdx", encodeMDX(*mannequin)));

	ASSERT_EQ(loaded->name, mannequin->name);
	ASSERT_EQ(loaded->numFrames(), mannequin->numFrames());
	ASSERT_EQ(loaded->joints.size(), mannequin->joints.size());
	ASSERT_EQ(loaded->torsoParent, mannequin->torsoParent);
	for (size_t i = 0; i < mannequin->joints.size(); i++)
	{
		ASSERT_EQ(loaded->joints[i].name, mannequin->joints[i].name);
		ASSERT_EQ(loaded->joints[i].parentId, mannequin->joints[i].parentId);
	}

	for (size_t m = 0; m < mannequin->meshes.size(); m++)
	{
		ASSERT_EQ(loaded->meshes[m].name, mannequin->meshes[m].name);
		ASSERT_EQ(loaded->meshes[m].tris, mannequin->meshes[m].tris);
		ASSERT_EQ(loaded->meshes[m].collapseMap, mannequin->meshes[m].collapseMap);
		ASSERT_EQ(loaded->meshes[m].minLod, mannequin->meshes[m].minLod);
		// MDM normals are written back as they were read
		ASSERT_EQ(loaded->meshes[m].vertices[3].normal, mannequin->meshes[m].vertices[3].normal);
	}

	// The bone angles and directions are stored as 16 bit angles, so the result is close but not exact
	for (int frame : {0, 30, 70, 110, 115})
	{
		auto a = pose(*mannequin, frame);
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
	auto bare = loadModel<SkeletalModel>(data.loadFile("models/mdm/mannequin.mdm"));
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
	auto mannequin = loadMannequin();
	auto loaded = loadModel<SkeletalModel>(memoryFile("mannequin.mds", encodeMDS(*mannequin)));
	ASSERT_TRUE(loaded);

	ASSERT_EQ(loaded->numFrames(), mannequin->numFrames());
	// MDS tags are bones: the tags with an offset from their bone become new tag bones, the others use their bone
	const auto withOffset = std::count_if(mannequin->tags.begin(), mannequin->tags.end(), [](const BoneTag &tag) {
		return glm::length(tag.location) > 1e-4f || std::abs(std::abs(tag.rotation.w) - 1.0f) > 1e-5f;
	});
	ASSERT_EQ(withOffset, 15); // tag_head and tag_torso sit on their bones
	ASSERT_EQ(loaded->joints.size(), mannequin->joints.size() + size_t(withOffset));
	ASSERT_EQ(loaded->tags.size(), 17);

	for (int frame : {0, 100})
	{
		auto a = pose(*mannequin, frame);
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
	// The mannequin has no bind pose, the first frame becomes the MD5 bind pose
	auto mannequin = loadMannequin();
	mannequin->firstExportFrame = 100;
	mannequin->lastExportFrame = 109;

	auto loaded = loadModel<SkeletalModel>(memoryFile("mannequin.md5mesh", encodeMD5Mesh(*mannequin)));
	model::io::loadAnimation(loaded.get(), memoryFile("mannequin.md5anim", encodeMD5Anim(*mannequin)));

	ASSERT_EQ(loaded->numFrames(), 10);
	EXPECT_LT(maxVertexDistance(pose(*mannequin, 100), pose(*loaded, FRAME_BIND_POSE)), 1e-3f);
	EXPECT_LT(maxVertexDistance(pose(*mannequin, 105), pose(*loaded, 5)), 1e-3f);
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
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/robot/head.md3"));
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
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/robot/head.md3"));
	addMorphLod(*head, data.loadFile("models/players/robot/head_1.md3"));

	TempFolder folder;
	writeMD3(*head, folder.file("head.md3"));
	ASSERT_TRUE(std::filesystem::exists(folder.file("head.md3")));
	ASSERT_TRUE(std::filesystem::exists(folder.file("head_1.md3")));

	mt::io::MTPath written(folder.path.string());
	auto lod = loadModel<morph::MorphModel>(written.loadFile("head_1.md3"));
	ASSERT_EQ(lod->meshes[0].name, "h_head");
	ASSERT_EQ(lod->meshes[0].numVertices(), 111); // the level, not the base mesh (165 vertices)

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
	auto mannequin = loadMannequin();
	auto original = pose(*mannequin, 100);

	mannequin->firstExportFrame = 95; // fewer frames to keep the test fast
	mannequin->lastExportFrame = 105;
	mannequin->scale = 2.0f;
	mannequin->transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1});
	mannequin->transform.location = {10, 0, 5};
	const glm::mat4 transform
		= glm::translate(mannequin->transform.location) * glm::toMat4(mannequin->transform.rotation) * glm::scale(glm::vec3(2.0f));

	auto loaded = loadModel<SkeletalModel>(memoryFile("mannequin.mdm", encodeMDM(*mannequin)));
	model::io::loadAnimation(loaded.get(), memoryFile("mannequin.mdx", encodeMDX(*mannequin)));
	EXPECT_LT(maxVertexDistance(original, pose(*loaded, 5), transform), 0.1f);

	auto md5 = loadModel<SkeletalModel>(memoryFile("mannequin.md5mesh", encodeMD5Mesh(*mannequin)));
	model::io::loadAnimation(md5.get(), memoryFile("mannequin.md5anim", encodeMD5Anim(*mannequin)));
	EXPECT_LT(maxVertexDistance(original, pose(*md5, 5), transform), 1e-3f);

	// The model itself is not changed
	ASSERT_FLOAT_EQ(mannequin->scale, 2.0f);
}

TEST(ExportTest, exportFrames)
{
	auto mannequin = loadMannequin();
	mannequin->firstExportFrame = 32;
	mannequin->lastExportFrame = 63;

	auto loaded = loadModel<SkeletalModel>(memoryFile("mannequin.mdm", encodeMDM(*mannequin)));
	model::io::loadAnimation(loaded.get(), memoryFile("mannequin.mdx", encodeMDX(*mannequin)));
	model::io::loadAnimation(loaded.get(), memoryFile("mannequin.aninc", encodeAninc(*mannequin)));

	ASSERT_EQ(loaded->numFrames(), 32);
	EXPECT_LT(maxVertexDistance(pose(*mannequin, 40), pose(*loaded, 8)), 0.05f);

	// Only the sequences in the range are kept, moved to the new frame numbers
	for (const auto &animation : loaded->animations)
	{
		ASSERT_GE(animation.startFrame, 0);
		ASSERT_LT(animation.lastFrame(), 32);
	}
	ASSERT_FALSE(loaded->animations.empty());
	ASSERT_LT(loaded->animations.size(), mannequin->animations.size());
}

TEST(ExportTest, cropFramesClipsSequences)
{
	auto mannequin = loadMannequin();
	mannequin->animations = {{"inside", 10, 5, 20, true}, {"across", 5, 10, 20}, {"outside", 50, 5, 20}};
	Animation reversed{"back", 30, 15, 20};
	reversed.reversed = true; // frames 30 down to 16
	mannequin->animations.push_back(reversed);

	cropFrames(*mannequin, 8, 20);
	ASSERT_EQ(mannequin->numFrames(), 13);
	ASSERT_EQ(mannequin->animations.size(), 3);
	ASSERT_EQ(mannequin->animations[0].startFrame, 2);
	ASSERT_EQ(mannequin->animations[0].numFrames, 5);
	ASSERT_EQ(mannequin->animations[1].startFrame, 0); // frames 8 - 14
	ASSERT_EQ(mannequin->animations[1].numFrames, 7);
	ASSERT_EQ(mannequin->animations[2].name, "back"); // frames 20 down to 16
	ASSERT_EQ(mannequin->animations[2].startFrame, 12);
	ASSERT_EQ(mannequin->animations[2].lastFrame(), 8);
}

TEST(ExportTest, aninc)
{
	auto mannequin = loadMannequin();
	mannequin->animations[3].reversed = true;
	mannequin->animations[3].moveSpeed = 160;

	auto loaded = loadMannequin();
	model::io::loadAnimation(loaded.get(), memoryFile("mannequin.aninc", encodeAninc(*mannequin)));
	ASSERT_EQ(loaded->animations.size(), mannequin->animations.size());
	for (size_t i = 0; i < mannequin->animations.size(); i++)
	{
		const auto &a = mannequin->animations[i];
		const auto &b = loaded->animations[i];
		ASSERT_EQ(b.name, a.name);
		ASSERT_EQ(b.startFrame, a.startFrame);
		ASSERT_EQ(b.numFrames, a.numFrames);
		ASSERT_EQ(b.loop, a.loop);
		ASSERT_EQ(b.fps, a.fps);
		ASSERT_EQ(b.moveSpeed, a.moveSpeed);
		ASSERT_EQ(b.reversed, a.reversed);
	}

	mannequin->animations[0].name = "has space";
	ASSERT_THROW(encodeAninc(*mannequin), mt_exception);
}

TEST(ExportTest, tagsFile)
{
	auto mannequin = loadMannequin();
	auto target = loadMannequin();
	target->tags.clear();

	loadTags(*target, memoryFile("mannequin.tag", encodeTags(*mannequin)));
	ASSERT_EQ(target->tags.size(), mannequin->tags.size());
	for (size_t i = 0; i < mannequin->tags.size(); i++)
	{
		ASSERT_EQ(target->tags[i].name, mannequin->tags[i].name);
		ASSERT_EQ(target->tags[i].parentBone, mannequin->tags[i].parentBone);
		EXPECT_LT(glm::distance(target->tags[i].location, mannequin->tags[i].location), 1e-5f);
		EXPECT_GT(std::abs(glm::dot(target->tags[i].rotation, mannequin->tags[i].rotation)), 0.99999f);
	}
}

TEST(ExportTest, stl)
{
	mt::io::MTPath data("data");
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/robot/head.md3"));
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
	auto mannequin = loadMannequin();
	TempFolder folder;

	model::io::writeModel(*mannequin, folder.file("mannequin.mdm"));
	model::io::writeAnimation(*mannequin, folder.file("mannequin.mdx"));
	model::io::writeAnimation(*mannequin, folder.file("mannequin.aninc"));

	mt::io::MTPath written(folder.path.string());
	auto loaded = loadModel<SkeletalModel>(written.loadFile("mannequin.mdm"));
	model::io::loadAnimation(loaded.get(), written.loadFile("mannequin.mdx"));
	model::io::loadAnimation(loaded.get(), written.loadFile("mannequin.aninc"));
	ASSERT_EQ(loaded->numFrames(), mannequin->numFrames());
	ASSERT_EQ(loaded->animations.size(), mannequin->animations.size());

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
	auto mannequin = loadMannequin();
	mannequin->meshes[0].name = std::string(80, 'x');
	ASSERT_THROW(encodeMDM(*mannequin), mt_exception);

	// Morph models are not skeletal
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/robot/head.md3"));
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

TEST(ExportTest, mdcRoundTrip)
{
	mt::io::MTPath data("data");
	auto tube = loadModel<morph::MorphModel>(data.loadFile("models/md3/tube.md3"));

	// Full frames: the same as MD3
	auto full = loadModel<morph::MorphModel>(memoryFile("tube.mdc", encodeMDC(*tube)));
	ASSERT_TRUE(full);
	ASSERT_EQ(full->numFrames(), 3);
	ASSERT_EQ(full->meshes[0].name, "w_tube");
	ASSERT_EQ(full->meshes[0].tris, tube->meshes[0].tris);
	for (int frame = 0; frame < 3; frame++)
	{
		for (size_t v = 0; v < tube->meshes[0].numVertices(); v++)
		{
			ASSERT_EQ(full->meshes[0].frames[frame][v].pos, tube->meshes[0].frames[frame][v].pos);
		}
		const Point &a = tube->tags[0].frames[frame], &b = full->tags[0].frames[frame];
		EXPECT_LT(glm::distance(a.location, b.location), 0.02f);
		EXPECT_GT(std::abs(glm::dot(a.rotation, b.rotation)), 0.9999f);
	}

	// Compressed frames are within 0.05 units, and smaller
	MdcOptions compress;
	compress.compress = true;
	auto compressedData = encodeMDC(*tube, 0, compress);
	EXPECT_LT(compressedData.size(), encodeMDC(*tube).size());
	auto compressed = loadModel<morph::MorphModel>(memoryFile("tube.mdc", compressedData));
	for (int frame = 0; frame < 3; frame++)
	{
		for (size_t v = 0; v < tube->meshes[0].numVertices(); v++)
		{
			ASSERT_LT(glm::distance(compressed->meshes[0].frames[frame][v].pos, tube->meshes[0].frames[frame][v].pos), 0.05f);
		}
	}

	// A frame that moves further than the compression reaches is stored as a full frame
	auto moved = *tube;
	for (auto &vert : moved.meshes[0].frames[2])
	{
		vert.pos.x += 20.0f;
	}
	auto far = loadModel<morph::MorphModel>(memoryFile("moved.mdc", encodeMDC(moved, 0, compress)));
	ASSERT_EQ(far->meshes[0].frames[2][0].pos, moved.meshes[0].frames[2][0].pos);

	// Skeletal models are baked into frames like for MD3
	auto mannequin = loadMannequin();
	mannequin->firstExportFrame = 20;
	mannequin->lastExportFrame = 25;
	ASSERT_THROW(encodeMDC(*mannequin), mt_exception); // 17 tags, MDC allows 16 like MD3
	mannequin->tags.pop_back();
	auto baked = loadModel<morph::MorphModel>(memoryFile("mannequin.mdc", encodeMDC(*mannequin)));
	ASSERT_EQ(baked->numFrames(), 6);
	ASSERT_EQ(baked->tags.size(), 16);

	// The level of detail files
	auto head = loadModel<morph::MorphModel>(data.loadFile("models/players/robot/head.md3"));
	addMorphLod(*head, data.loadFile("models/players/robot/head_1.md3"));
	TempFolder folder;
	writeMDC(*head, folder.file("head.mdc"));
	ASSERT_TRUE(std::filesystem::exists(folder.file("head_1.mdc")));
}

TEST(ExportTest, ms3dRoundTrip)
{
	auto mannequin = loadMannequin();
	mannequin->firstExportFrame = 0;
	mannequin->lastExportFrame = 115;

	auto loaded = loadModel<SkeletalModel>(memoryFile("mannequin.ms3d", encodeMS3D(*mannequin)));
	ASSERT_TRUE(loaded);
	ASSERT_EQ(loaded->name, "mannequin");
	ASSERT_EQ(loaded->numFrames(), 116);
	ASSERT_EQ(loaded->animations.size(), 1);
	ASSERT_EQ(loaded->animations[0].fps, 20);
	ASSERT_EQ(loaded->meshes.size(), 4);
	ASSERT_EQ(loaded->meshes[0].name, "u_body");

	// The tags are tag bones after the bones, the loader marks them
	ASSERT_EQ(loaded->joints.size(), mannequin->joints.size() + mannequin->tags.size());
	ASSERT_EQ(std::count_if(loaded->joints.begin(), loaded->joints.end(), [](const Joint &joint) { return joint.asTag; }), 17);
	ASSERT_TRUE(et::checkArmature(*loaded).empty());

	// The file is Y-up, the loader turns it back. MS3D has one bind position per vertex: the MDM has no bind pose, so
	// the writer binds at frame 0. That pose is exact, the other frames blend the bones from the new bind pose and
	// land close by (linear blend skinning is not the same after a new bind pose).
	bakeTransform(*loaded);
	auto compare = [&](const SkeletalModel &expected, const SkeletalModel &actual, int frame, float vertexTolerance) {
		auto a = pose(expected, frame);
		auto b = pose(actual, frame);
		ASSERT_EQ(a.meshes.size(), b.meshes.size());
		// The loader orders the vertices by the triangles, every vertex is compared with the nearest one
		float worst = 0;
		for (size_t m = 0; m < a.meshes.size(); m++)
		{
			for (const auto &position : b.meshes[m].positions)
			{
				float nearest = std::numeric_limits<float>::max();
				for (const auto &other : a.meshes[m].positions)
				{
					nearest = std::min(nearest, glm::distance(position, other));
				}
				worst = std::max(worst, nearest);
			}
		}
		EXPECT_LT(worst, vertexTolerance) << "frame " << frame;

		for (const auto &tag : a.tags)
		{
			auto *other = findTag(b, tag.name);
			ASSERT_TRUE(other) << tag.name;
			EXPECT_LT(glm::distance(tag.point.location, other->point.location), 1e-3f) << tag.name;
			EXPECT_GT(std::abs(glm::dot(tag.point.rotation, other->point.rotation)), 0.9999f) << tag.name;
		}
	};
	compare(*mannequin, *loaded, 0, 1e-3f);
	for (int frame : {30, 70, 110})
	{
		compare(*mannequin, *loaded, frame, 0.1f);
	}

	// A model with a bind pose comes back exactly in every frame
	mt::io::MTPath fixtures("data");
	auto ms3d = loadModel<SkeletalModel>(fixtures.loadFile("models/ms3d/test.ms3d"));
	bakeTransform(*ms3d);
	auto again = loadModel<SkeletalModel>(memoryFile("again.ms3d", encodeMS3D(*ms3d)));
	bakeTransform(*again);
	ASSERT_EQ(again->joints.size(), ms3d->joints.size());
	for (int frame : {0, 30, 70, 95})
	{
		compare(*ms3d, *again, frame, 1e-3f);
	}

	// Errors
	mt::io::MTPath data("data");
	auto tube = loadModel<morph::MorphModel>(data.loadFile("models/md3/tube.md3"));
	ASSERT_THROW(encodeMS3D(*tube), mt_exception);
	auto longName = loadMannequin();
	longName->meshes[0].name = std::string(40, 'x');
	ASSERT_THROW(encodeMS3D(*longName), mt_exception);

	// Through the registry
	TempFolder folder;
	model::io::writeModel(*mannequin, folder.file("mannequin.ms3d"));
	model::io::writeModel(*tube, folder.file("tube.mdc"));
	ASSERT_TRUE(std::filesystem::exists(folder.file("mannequin.ms3d")));
	ASSERT_TRUE(std::filesystem::exists(folder.file("tube.mdc")));
}
