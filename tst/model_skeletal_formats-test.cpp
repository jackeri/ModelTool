#include "gtest/gtest.h"
#include "model_mdm.h"
#include "model_mdx.h"
#include "model_scripts.h"
#include "skeletal_model.h"
#include "morph_model.h"
#include "hitbox.h"
#include "idtech.h"
#include "mt_path.h"

#include <cstring>

using namespace mt;
using namespace mt::model;
using namespace mt::model::skeletal;

namespace {
	Ref<mt::io::MTFile> textFile(const std::string &name, const std::string &text)
	{
		auto file = make_ref<mt::io::MTFile>();
		file->data = make_ref<std::vector<byte>>(text.begin(), text.end());
		file->data->push_back(0);
		file->len = text.size();
		file->name = name;
		file->ext = model::io::normalizeExtension(name);
		return file;
	}

	Ref<mt::io::MTFile> binaryFile(const std::string &name, const std::vector<byte> &data)
	{
		auto file = make_ref<mt::io::MTFile>();
		file->data = make_ref<std::vector<byte>>(data);
		file->len = data.size();
		file->name = name;
		file->ext = model::io::normalizeExtension(name);
		return file;
	}

	std::unique_ptr<SkeletalModel> loadMannequin(bool withAnimation = true)
	{
		mt::io::MTPath data("data");
		std::unique_ptr<Model> loaded(model::io::loadModel(data.loadFile("models/mdm/mannequin.mdm")));
		if (withAnimation)
		{
			model::io::loadAnimation(loaded.get(), data.loadFile("models/mdm/mannequin.mdx"));
		}
		return std::unique_ptr<SkeletalModel>(dynamic_cast<SkeletalModel *>(loaded.release()));
	}

	glm::vec3 posedVertex(const SkeletalModel &model, const Mesh &mesh, const SkeletalVertex &vert, int frame, bool inverse = false)
	{
		glm::vec3 pos{0, 0, 0};
		for (int i = 0; i < vert.weightCount; i++)
		{
			const auto &weight = mesh.weights[vert.startWeight + i];
			const auto &joint = model.joints[weight.jointId].frames[frame];
			glm::quat rotation = inverse ? glm::inverse(joint.rotation) : joint.rotation;
			pos += (joint.location + rotation * weight.pos) * weight.bias;
		}
		return pos;
	}

	template<typename T>
	void append(std::vector<byte> &data, const T &value)
	{
		const auto *bytes = reinterpret_cast<const byte *>(&value);
		data.insert(data.end(), bytes, bytes + sizeof(T));
	}

	template<size_t N>
	void setName(char (&field)[N], const std::string &value)
	{
		std::memset(field, 0, N);
		std::strncpy(field, value.c_str(), N - 1);
	}

	/*! Small MDS file: two bones, one frame, one surface with a triangle, one tag */
	std::vector<byte> syntheticMDS()
	{
		using namespace mt::idtech;

		const int numBones = 2;
		const int numVerts = 3;
		const int vertexSize = sizeof(mdsVertexHeader_t) + sizeof(skelWeight_t);

		mdsHeader_t header{};
		header.ident = MDS_IDENT;
		header.version = MDS_VERSION;
		setName(header.name, "synthetic");
		header.lodScale = 1.0f;
		header.lodBias = 0.5f;
		header.numFrames = 1;
		header.numBones = numBones;
		header.torsoParent = 0;
		header.numSurfaces = 1;
		header.numTags = 1;
		header.ofsBones = sizeof(mdsHeader_t);
		header.ofsFrames = header.ofsBones + numBones * int(sizeof(skelBoneInfo_t));
		header.ofsSurfaces = header.ofsFrames + int(sizeof(mdsFrameHeader_t)) + numBones * int(sizeof(skelBoneFrameCompressed_t));

		mdsSurface_t surface{};
		setName(surface.name, "body");
		setName(surface.shader, "skin.tga");
		surface.minLod = 2;
		surface.ofsHeader = -header.ofsSurfaces;
		surface.numVerts = numVerts;
		surface.ofsVerts = sizeof(mdsSurface_t);
		surface.numTriangles = 1;
		surface.ofsTriangles = surface.ofsVerts + numVerts * vertexSize;
		surface.ofsCollapseMap = surface.ofsTriangles + int(sizeof(skelTriangle_t));
		surface.ofsBoneReferences = surface.ofsCollapseMap + numVerts * 4;
		surface.ofsEnd = surface.ofsBoneReferences;

		header.ofsTags = header.ofsSurfaces + surface.ofsEnd;
		header.ofsEnd = header.ofsTags + int(sizeof(mdsTag_t));

		std::vector<byte> data;
		append(data, header);

		skelBoneInfo_t root{};
		setName(root.name, "root");
		root.parent = -1;
		append(data, root);

		skelBoneInfo_t child{};
		setName(child.name, "child");
		child.parent = 0;
		child.parentDist = 10.0f;
		child.torsoWeight = 0.5f;
		child.flags = BONEFLAG_TAG;
		append(data, child);

		mdsFrameHeader_t frame{};
		frame.bounds[0][0] = 20; // max corner first, the loader sorts the corners
		frame.parentOffset[0] = 1;
		frame.parentOffset[1] = 2;
		frame.parentOffset[2] = 3;
		append(data, frame);

		skelBoneFrameCompressed_t rootFrame{};
		append(data, rootFrame);

		skelBoneFrameCompressed_t childFrame{};
		childFrame.angles[YAW] = 16384; // 90 degrees
		append(data, childFrame);		// ofsAngles 0, 0: the child is straight forward (+x) from the root

		append(data, surface);
		for (int v = 0; v < numVerts; v++)
		{
			mdsVertexHeader_t vert{};
			vert.normal[2] = 1;
			vert.texCoords[0] = float(v);
			vert.numWeights = 1;
			append(data, vert);

			skelWeight_t weight{};
			weight.boneIndex = 1;
			weight.boneWeight = 1;
			weight.offset[0] = 1;
			weight.offset[1] = float(v);
			append(data, weight);
		}
		append(data, skelTriangle_t{{0, 1, 2}});
		for (int v = 0; v < numVerts; v++)
		{
			append(data, int32_t(v));
		}

		mdsTag_t tag{};
		setName(tag.name, "tag_child");
		tag.torsoWeight = 0.25f;
		tag.boneIndex = 1;
		append(data, tag);

		return data;
	}
}

TEST(SkeletalFormatsTest, mdmWithoutAnimation)
{
	auto model = loadMannequin(false);
	ASSERT_TRUE(model);

	// Generated test model (tools/fixtures): a mannequin on the default ET armature
	ASSERT_EQ(model->name, "models/players/mannequin/body.mdm");
	ASSERT_FALSE(model->hasBindPose);
	ASSERT_FALSE(model->hasFrames());
	ASSERT_TRUE(model->joints.empty());
	ASSERT_FLOAT_EQ(model->lodScale, 1.0f);

	ASSERT_EQ(model->meshes.size(), 4);
	const auto &body = model->meshes[0];
	ASSERT_EQ(body.name, "u_body");
	ASSERT_EQ(body.vertices.size(), 384);
	ASSERT_EQ(body.tris.size(), 320);
	ASSERT_EQ(body.weights.size(), 564);
	ASSERT_EQ(body.minLod, 364);
	ASSERT_EQ(body.collapseMap.size(), body.vertices.size());

	ASSERT_EQ(model->tags.size(), 17);
	ASSERT_EQ(model->tags[0].name, "tag_head");
}

TEST(SkeletalFormatsTest, mdxAnimation)
{
	auto model = loadMannequin();
	ASSERT_TRUE(model);

	ASSERT_EQ(model->joints.size(), 53);
	ASSERT_EQ(model->joints[0].name, "Bip01 Pelvis");
	ASSERT_EQ(model->joints[0].parentId, -1);
	ASSERT_EQ(model->torsoParent, 1);
	ASSERT_EQ(model->numFrames(), 116);
	ASSERT_EQ(model->bounds.size(), 116);
	ASSERT_EQ(model->currentFrame, 0);

	for (const auto &bounds : model->bounds)
	{
		ASSERT_LE(bounds.min.x, bounds.max.x);
		ASSERT_LE(bounds.min.z, bounds.max.z);
	}

	// Child bones are placed at their parent distance
	for (const auto &joint : model->joints)
	{
		if (joint.parentId >= 0)
		{
			const auto &parent = model->joints[joint.parentId];
			ASSERT_NEAR(glm::distance(joint.frames[10].location, parent.frames[10].location), joint.parentDist, 1e-3f);
		}
	}

	// The tags are at sensible places: the head is above the feet
	auto tagPosition = [&](const std::string &name) {
		auto tag = std::find_if(model->tags.begin(), model->tags.end(), [&](const BoneTag &t) { return t.name == name; });
		const Point &bone = model->joints[tag->parentBone].frames[0];
		return bone.location + bone.rotation * tag->location;
	};
	ASSERT_GT(tagPosition("tag_head").z, 20.0f);
	ASSERT_LT(tagPosition("tag_footleft").z, -20.0f);
}

TEST(SkeletalFormatsTest, boneRotationConvention)
{
	// ET applies the bone matrices with the axis as rows. With the right convention the weights of a vertex agree on
	// where the vertex is, with the transposed matrices they are clearly further apart. Frame 110 is the fallen pose
	// of the death sequence, the bones are turned a lot there.
	auto model = loadMannequin();
	const int frame = 110;

	auto spread = [&](bool inverse) {
		double total = 0;
		int count = 0;
		for (const auto &mesh : model->meshes)
		{
			for (const auto &vert : mesh.vertices)
			{
				if (vert.weightCount < 2)
				{
					continue;
				}

				glm::vec3 pos = posedVertex(*model, mesh, vert, frame, inverse);
				for (int i = 0; i < vert.weightCount; i++)
				{
					const auto &weight = mesh.weights[vert.startWeight + i];
					const auto &joint = model->joints[weight.jointId].frames[frame];
					glm::quat rotation = inverse ? glm::inverse(joint.rotation) : joint.rotation;
					total += glm::distance(joint.location + rotation * weight.pos, pos);
					count++;
				}
			}
		}
		return total / count;
	};

	ASSERT_LT(spread(false), 0.1);
	ASSERT_GT(spread(true), 2.0);

	// Every vertex lies inside the frame bounds stored in the MDX
	for (int frame : {0, 50, 110})
	{
		const auto &bounds = model->bounds[frame];
		for (const auto &mesh : model->meshes)
		{
			for (const auto &vert : mesh.vertices)
			{
				glm::vec3 pos = posedVertex(*model, mesh, vert, frame);
				ASSERT_TRUE(glm::all(glm::greaterThanEqual(pos, bounds.min - 0.5f))
							&& glm::all(glm::lessThanEqual(pos, bounds.max + 0.5f)));
			}
		}
	}
}

TEST(SkeletalFormatsTest, mdxErrors)
{
	mt::io::MTPath data("data");

	// The bones need to match the model
	auto model = loadMannequin();
	model->joints[5].name = "renamed";
	auto frames = model->numFrames();
	ASSERT_THROW(model::io::loadAnimation(model.get(), data.loadFile("models/mdm/mannequin.mdx")), mt_exception);
	// The model is left as it was
	ASSERT_EQ(model->joints[5].name, "renamed");
	ASSERT_EQ(model->numFrames(), frames);

	// MDX needs a skeletal model
	morph::MorphModel morph;
	ASSERT_THROW(model::io::loadAnimation(&morph, data.loadFile("models/mdm/mannequin.mdx")), mt_exception);

	// MDX alone loads as a skeleton
	std::unique_ptr<Model> skeleton(model::io::loadModel(data.loadFile("models/mdm/mannequin.mdx")));
	auto *skeletal = dynamic_cast<SkeletalModel *>(skeleton.get());
	ASSERT_TRUE(skeletal);
	ASSERT_TRUE(skeletal->meshes.empty());
	ASSERT_EQ(skeletal->joints.size(), 53);

	// Truncated animation
	auto file = data.loadFile("models/mdm/mannequin.mdx");
	file->len /= 2;
	auto mannequin = loadMannequin(false);
	ASSERT_THROW(model::io::loadAnimation(mannequin.get(), file), mt_exception);
	ASSERT_TRUE(mannequin->joints.empty());
}

TEST(SkeletalFormatsTest, mdmErrors)
{
	mt::io::MTPath data("data");

	auto file = data.loadFile("models/mdm/mannequin.mdm");
	file->len = 1000;
	ASSERT_THROW(loadMDM(file), mt_exception);

	ASSERT_THROW(loadMDM(data.loadFile("models/mdm/mannequin.mdx")), mt_exception);
}

TEST(SkeletalFormatsTest, mds)
{
	std::unique_ptr<Model> loaded(model::io::loadModel(binaryFile("synthetic.mds", syntheticMDS())));
	auto *model = dynamic_cast<SkeletalModel *>(loaded.get());
	ASSERT_TRUE(model);

	ASSERT_EQ(model->name, "synthetic");
	ASSERT_FLOAT_EQ(model->lodBias, 0.5f);
	ASSERT_EQ(model->torsoParent, 0);
	ASSERT_EQ(model->numFrames(), 1);
	ASSERT_EQ(model->currentFrame, 0);
	ASSERT_EQ(model->bounds[0].max.x, 20);
	ASSERT_EQ(model->bounds[0].min.x, 0);

	ASSERT_EQ(model->joints.size(), 2);
	const auto &child = model->joints[1];
	ASSERT_EQ(child.name, "child");
	ASSERT_TRUE(child.asTag);
	ASSERT_FLOAT_EQ(child.torsoWeight, 0.5f);
	ASSERT_FLOAT_EQ(child.parentDist, 10.0f);

	// Root at the parent offset, child 10 units forward from it
	glm::vec3 rootPos = model->joints[0].frames[0].location;
	glm::vec3 childPos = child.frames[0].location;
	ASSERT_NEAR(rootPos.x, 1, 1e-5f);
	ASSERT_NEAR(childPos.x, 11, 1e-5f);
	ASSERT_NEAR(childPos.y, 2, 1e-5f);
	ASSERT_NEAR(childPos.z, 3, 1e-5f);

	const auto &mesh = model->meshes[0];
	ASSERT_EQ(mesh.name, "body");
	ASSERT_EQ(mesh.materialName, "skin.tga");
	ASSERT_EQ(mesh.minLod, 2);
	ASSERT_EQ(mesh.vertices.size(), 3);
	ASSERT_EQ(mesh.tris[0], glm::ivec3(0, 1, 2));
	ASSERT_EQ(mesh.collapseMap, std::vector<int>({0, 1, 2}));
	ASSERT_FLOAT_EQ(mesh.vertices[2].tex.x, 2);

	// The child is turned 90 degrees, the bone matrix is applied transposed like in the game: (1, 0, 0) -> (0, -1, 0)
	glm::vec3 vert = posedVertex(*model, mesh, mesh.vertices[0], 0);
	ASSERT_NEAR(vert.x, 11, 1e-4f);
	ASSERT_NEAR(vert.y, 1, 1e-4f);
	ASSERT_NEAR(vert.z, 3, 1e-4f);

	ASSERT_EQ(model->tags.size(), 1);
	ASSERT_EQ(model->tags[0].name, "tag_child");
	ASSERT_EQ(model->tags[0].parentBone, 1);
	ASSERT_FLOAT_EQ(model->tags[0].torsoWeight, 0.25f);
}

TEST(SkeletalFormatsTest, mdsErrors)
{
	auto data = syntheticMDS();

	// Weight pointing to a bone that does not exist
	auto badWeight = data;
	const size_t weightOffset = sizeof(idtech::mdsHeader_t) + 2 * sizeof(idtech::skelBoneInfo_t) + sizeof(idtech::mdsFrameHeader_t)
								+ 2 * sizeof(idtech::skelBoneFrameCompressed_t) + sizeof(idtech::mdsSurface_t)
								+ sizeof(idtech::mdsVertexHeader_t);
	int32_t bone = 7;
	std::memcpy(badWeight.data() + weightOffset, &bone, sizeof(bone));
	ASSERT_THROW(loadMDS(binaryFile("bad.mds", badWeight)), mt_exception);

	// Parent bone after the child
	auto badParent = data;
	int32_t parent = 1;
	std::memcpy(badParent.data() + sizeof(idtech::mdsHeader_t) + offsetof(idtech::skelBoneInfo_t, parent), &parent, sizeof(parent));
	ASSERT_THROW(loadMDS(binaryFile("bad.mds", badParent)), mt_exception);

	// Truncated
	data.resize(data.size() - 10);
	ASSERT_THROW(loadMDS(binaryFile("bad.mds", data)), mt_exception);
}

TEST(SkeletalFormatsTest, aninc)
{
	auto model = loadMannequin();
	mt::io::MTPath data("data");
	model::io::loadAnimation(model.get(), data.loadFile("models/mdm/mannequin.aninc"));

	ASSERT_EQ(model->animations.size(), 7);
	const auto &first = model->animations[0];
	ASSERT_EQ(first.name, "idle");
	ASSERT_EQ(first.startFrame, 0);
	ASSERT_EQ(first.numFrames, 20);
	ASSERT_TRUE(first.loop);
	ASSERT_EQ(first.fps, 20);
	ASSERT_EQ(model->animations[1].name, "walk");
	ASSERT_EQ(model->animations[1].moveSpeed, 60);
	ASSERT_EQ(model->animations[1].transition, 100);
	ASSERT_FALSE(model->animations[3].loop); // wave
	ASSERT_EQ(model->animations.back().name, "dead");

	for (const auto &animation : model->animations)
	{
		ASSERT_LT(animation.lastFrame(), model->numFrames()) << animation.name;
	}

	// Loading again replaces the sequences
	model::io::loadAnimation(model.get(), textFile("test.aninc", "run 10 5 1 15 120 100 1\n"));
	ASSERT_EQ(model->animations.size(), 1);
	const auto &run = model->animations[0];
	ASSERT_EQ(run.moveSpeed, 120);
	ASSERT_EQ(run.transition, 100);
	ASSERT_TRUE(run.reversed);
	ASSERT_EQ(run.lastFrame(), 6);

	// Missing columns
	ASSERT_THROW(model::io::loadAnimation(model.get(), textFile("bad.aninc", "run 10 5 1\nwalk 1 2 3 4 5 6 7\n")), mt_exception);
	ASSERT_THROW(model::io::loadAnimation(model.get(), textFile("bad.aninc", "run 10 five 1 15 0 0 0\n")), mt_exception);
	ASSERT_EQ(model->animations.size(), 1);
}

TEST(SkeletalFormatsTest, tagsFile)
{
	auto model = loadMannequin();
	size_t tags = model->tags.size();

	loadTags(*model, textFile("mannequin.tag", "// File Generated by LegacyModelView\n"
											   "tags {\n"
											   "\t\"tag_hat\" \"Bip01 Head\" ( 1 2 3 ) ( 0 0 0 1 )\n"
											   "\t\"tag_belt\" \"bip01 spine\" ( 0 0 0 ) ( 0 0 0.7071068 0.7071068 )\n"
											   "}\n"));

	ASSERT_EQ(model->tags.size(), tags + 2);
	const auto &hat = model->tags[tags];
	ASSERT_EQ(hat.name, "tag_hat");
	ASSERT_EQ(model->joints[hat.parentBone].name, "Bip01 Head");
	ASSERT_EQ(hat.location, glm::vec3(1, 2, 3));
	ASSERT_EQ(hat.rotation, glm::quat(1, 0, 0, 0));

	// Bone names are not case sensitive, the quaternion is x y z w
	const auto &belt = model->tags[tags + 1];
	ASSERT_EQ(model->joints[belt.parentBone].name, "Bip01 Spine");
	ASSERT_NEAR(belt.rotation.z, 0.7071068f, 1e-6f);
	ASSERT_NEAR(belt.rotation.w, 0.7071068f, 1e-6f);

	ASSERT_THROW(loadTags(*model, textFile("bad.tag", "tags {\n\"tag_x\" \"no such bone\" ( 0 0 0 ) ( 0 0 0 1 )\n}\n")), mt_exception);
	ASSERT_THROW(loadTags(*model, textFile("bad.tag", "hitbox {\n}\n")), mt_exception);
	// Nothing is added from a broken file
	ASSERT_EQ(model->tags.size(), tags + 2);
}

TEST(SkeletalFormatsTest, legacyHitboxes)
{
	std::vector<Hitbox> hitboxes;
	Hitbox::loadHitboxes(textFile("mannequin.htx", "// File Generated by LegacyModelView\n"
												   "hitbox {\n"
												   "\tname \"head\"\n"
												   "\tmodifiers {\n"
												   "\t\theadShot true\n"
												   "\t}\n"
												   "\tlocation ( 1 2 3 ) ( 0 0 0 1 )\n"
												   "\tmin ( -4 -4 -4 )\n"
												   "\tmax ( 4 4 5 )\n"
												   "\tjoint 12\n"
												   "}\n"
												   "hitbox {\n"
												   "\tmodifiers {\n"
												   "\t\theadShot false\n"
												   "\t}\n"
												   "\tlocation ( 0 0 0 ) ( 0 0 0 1 )\n"
												   "\tmin ( -1 -1 -1 )\n"
												   "\tmax ( 1 1 1 )\n"
												   "\tjoint 0\n"
												   "}\n"),
						 hitboxes);

	ASSERT_EQ(hitboxes.size(), 2);
	const auto &head = hitboxes[0];
	ASSERT_EQ(head.name, "head");
	ASSERT_EQ(head.parentJoint, 12);
	ASSERT_EQ(head.location, glm::vec3(1, 2, 3));
	ASSERT_EQ(head.rotation, glm::quat(1, 0, 0, 0));
	ASSERT_EQ(head.min, glm::vec3(-4));
	ASSERT_EQ(head.max, glm::vec3(4, 4, 5));
	ASSERT_EQ(head.modifiers.count("headshot"), 1);

	ASSERT_TRUE(hitboxes[1].modifiers.empty());
	ASSERT_EQ(hitboxes[1].parentJoint, 0);
}
