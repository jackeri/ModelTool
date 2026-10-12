#include "model_mdm.h"
#include "model_mdx.h"
#include "binary_reader.h"
#include "binary_writer.h"
#include "model_export.h"
#include "skeletal_tools.h"
#include "lod.h"

#include <cstring>
#include <numeric>
#include <set>
#include "idtech.h"

namespace mt::model {
	using namespace mt::idtech;
	using namespace mt::model::skeletal;
	using mt::io::BinaryReader;

	static_assert(sizeof(mdmHeader_t) == 100);
	static_assert(sizeof(mdmSurface_t) == 176);
	static_assert(sizeof(mdmTag_t) == 128);
	static_assert(sizeof(mdmVertexHeader_t) == 24);
	static_assert(sizeof(skelWeight_t) == 20);
	static_assert(sizeof(mdsHeader_t) == 120);
	static_assert(sizeof(mdsSurface_t) == sizeof(mdmSurface_t));
	static_assert(sizeof(mdsVertexHeader_t) == 32);
	static_assert(sizeof(mdsTag_t) == 72);
	static_assert(sizeof(mdsFrameHeader_t) == sizeof(mdxFrame_t));

	/**
	 * Read the surfaces of an MDM or MDS file, the surface layout is the same but the vertex header differs
	 * @tparam VertexHeader mdmVertexHeader_t or mdsVertexHeader_t
	 */
	template<typename VertexHeader>
	static void readSurfaces(const BinaryReader &reader, int64_t offset, int32_t numSurfaces, SkeletalModel &model)
	{
		for (int32_t i = 0; i < numSurfaces; i++)
		{
			auto info = reader.readAt<mdmSurface_t>(offset);
			BinaryReader surface = reader.from(offset);

			Mesh mesh;
			mesh.name = BinaryReader::toString(info.name);
			mesh.materialName = BinaryReader::toString(info.shader);
			mesh.minLod = info.minLod;

			surface.seek(info.ofsVerts);
			if (info.numVerts < 0)
			{
				throw mt_ex("Invalid vertex count: " + std::to_string(info.numVerts));
			}

			for (int32_t v = 0; v < info.numVerts; v++)
			{
				auto header = surface.read<VertexHeader>();
				auto weights = surface.readArray<skelWeight_t>(header.numWeights);

				SkeletalVertex vertex;
				vertex.normal = toVec3(header.normal);
				vertex.tex = {header.texCoords[0], header.texCoords[1]};
				vertex.startWeight = int(mesh.weights.size());
				vertex.weightCount = header.numWeights;
				mesh.vertices.push_back(vertex);

				for (const auto &weight : weights)
				{
					mesh.weights.push_back({weight.boneIndex, weight.boneWeight, toVec3(weight.offset)});
				}
			}

			surface.seek(info.ofsTriangles);
			for (const auto &tri : surface.readArray<skelTriangle_t>(info.numTriangles))
			{
				for (int index : tri.indexes)
				{
					if (index < 0 || index >= info.numVerts)
					{
						throw mt_ex("Mesh " + mesh.name + " has a triangle pointing to an invalid vertex: " + std::to_string(index));
					}
				}
				mesh.tris.emplace_back(tri.indexes[0], tri.indexes[1], tri.indexes[2]);
			}

			surface.seek(info.ofsCollapseMap);
			mesh.collapseMap = surface.readArray<int32_t>(info.numVerts);

			surface.seek(info.ofsBoneReferences);
			mesh.boneReferences = surface.readArray<int32_t>(info.numBoneReferences);

			model.meshes.push_back(std::move(mesh));

			if (info.ofsEnd <= 0)
			{
				throw mt_ex("Invalid surface end offset: " + std::to_string(info.ofsEnd));
			}
			offset += info.ofsEnd;
		}
	}

	Model *loadMDM(const Ref<mt::io::MTFile> &file)
	{
		BinaryReader reader(file);
		auto header = reader.read<mdmHeader_t>();

		if (header.ident != MDM_IDENT)
		{
			throw mt_ex("Not an MDM file");
		}

		if (header.version != MDM_VERSION)
		{
			throw mt_ex("Unsupported MDM version: " + std::to_string(header.version));
		}

		auto model = std::make_unique<SkeletalModel>();
		model->name = BinaryReader::toString(header.name);
		model->lodScale = header.lodScale;
		model->lodBias = header.lodBias;

		readSurfaces<mdmVertexHeader_t>(reader, header.ofsSurfaces, header.numSurfaces, *model);

		int64_t offset = header.ofsTags;
		for (int32_t i = 0; i < header.numTags; i++)
		{
			auto tag = reader.readAt<mdmTag_t>(offset);

			BoneTag out;
			out.name = BinaryReader::toString(tag.name);
			out.parentBone = tag.boneIndex;
			// The tag axis is transformed by the bone matrix like the vertices, so it is a rotation relative to the bone
			out.location = toVec3(tag.offset);
			out.rotation = Point::fromAxis({}, toAxis(tag.axis)).rotation;
			out.boneReferences = reader.from(offset)
									 .sub(tag.ofsBoneReferences, int64_t(tag.numBoneReferences) * 4)
									 .readArray<int32_t>(tag.numBoneReferences);
			model->tags.push_back(out);

			if (tag.ofsEnd <= 0)
			{
				throw mt_ex("Invalid tag end offset: " + std::to_string(tag.ofsEnd));
			}
			offset += tag.ofsEnd;
		}

		return model.release();
	}

	Model *loadMDS(const Ref<mt::io::MTFile> &file)
	{
		BinaryReader reader(file);
		auto header = reader.read<mdsHeader_t>();

		if (header.ident != MDS_IDENT)
		{
			throw mt_ex("Not an MDS file");
		}

		if (header.version != MDS_VERSION)
		{
			throw mt_ex("Unsupported MDS version: " + std::to_string(header.version));
		}

		if (header.torsoParent < -1 || header.torsoParent >= header.numBones)
		{
			throw mt_ex("Invalid torso parent: " + std::to_string(header.torsoParent));
		}

		auto model = std::make_unique<SkeletalModel>();
		model->name = BinaryReader::toString(header.name);
		model->lodScale = header.lodScale;
		model->lodBias = header.lodBias;
		model->torsoParent = header.torsoParent;

		reader.seek(header.ofsBones);
		auto bones = reader.readArray<skelBoneInfo_t>(header.numBones);
		detail::createJoints(*model, bones);

		readSurfaces<mdsVertexHeader_t>(reader, header.ofsSurfaces, header.numSurfaces, *model);
		detail::readBoneFrames(reader, header.ofsFrames, header.numFrames, bones, *model);

		// MDS tags are bones, so they sit at the bone with the bone's rotation
		reader.seek(header.ofsTags);
		for (const auto &tag : reader.readArray<mdsTag_t>(header.numTags))
		{
			BoneTag out;
			out.name = BinaryReader::toString(tag.name);
			out.parentBone = tag.boneIndex;
			out.torsoWeight = tag.torsoWeight;
			model->tags.push_back(out);
		}

		detail::validateWeights(*model);

		if (model->hasFrames())
		{
			model->currentFrame = 0;
		}

		return model.release();
	}

	namespace {
		using mt::io::BinaryWriter;

		/**
		 * Bones needed to calculate the given bones: the bones and all their parents, parents first (sorted)
		 */
		std::vector<int32_t> boneReferences(const SkeletalModel &model, const detail::EtSkeleton &skeleton, const std::set<int> &bones,
											const std::vector<int> &fileReferences = {})
		{
			std::set<int32_t> references;

			// Without bones the parents are not known, keep the references read from the file
			if (model.joints.empty())
			{
				// Keep the file's order when it already has every bone
				std::set<int> fromFile(fileReferences.begin(), fileReferences.end());
				if (std::includes(fromFile.begin(), fromFile.end(), bones.begin(), bones.end()))
				{
					return {fileReferences.begin(), fileReferences.end()};
				}

				references.insert(bones.begin(), bones.end());
				references.insert(fileReferences.begin(), fileReferences.end());
				return {references.begin(), references.end()};
			}

			for (int bone : bones)
			{
				for (int current = bone; current >= 0; current = model.joints[current].parentId)
				{
					if (skeleton.remap[current] >= 0)
					{
						references.insert(skeleton.remap[current]);
					}
				}
			}
			return {references.begin(), references.end()};
		}

		/**
		 * Collapse maps are needed by the game's level of detail, calculate them when a mesh does not have a valid one
		 */
		void ensureCollapseMaps(SkeletalModel &model)
		{
			for (const auto &mesh : model.meshes)
			{
				bool valid = mesh.collapseMap.size() == mesh.vertices.size();
				for (size_t i = 1; valid && i < mesh.collapseMap.size(); i++)
				{
					valid = mesh.collapseMap[i] >= 0 && mesh.collapseMap[i] < int(i);
				}

				if (!valid)
				{
					calculateLod(model, model.hasBindPose || !model.hasFrames() ? FRAME_BIND_POSE : 0);
					return;
				}
			}
		}

		/**
		 * The game rotates a vertex normal with the first weight's bone, so the file stores it in that bone's space
		 */
		glm::vec3 fileNormal(const SkeletalModel &model, const Mesh &mesh, const SkeletalVertex &vert)
		{
			if (!model.hasBindPose || vert.weightCount <= 0)
			{
				return vert.normal;
			}
			const Joint &joint = model.joints.at(size_t(mesh.weights.at(size_t(vert.startWeight)).jointId));
			return glm::normalize(glm::inverse(joint.rotation) * vert.bindNormal);
		}

		/**
		 * Write the surfaces of MDM / MDS
		 * @tparam VertexHeader mdmVertexHeader_t or mdsVertexHeader_t
		 */
		template<typename VertexHeader>
		void writeSurfaces(BinaryWriter &out, const SkeletalModel &model, const detail::EtSkeleton &skeleton)
		{
			for (const auto &mesh : model.meshes)
			{
				const int32_t start = out.tell();

				mdmSurface_t surface{};
				BinaryWriter::setString(surface.name, mesh.name, "mesh name");
				BinaryWriter::setString(surface.shader, mesh.materialName, "material name");
				surface.minLod = std::clamp(mesh.minLod, 0, int(mesh.vertices.size()));
				surface.ofsHeader = -start;
				surface.numVerts = int32_t(mesh.vertices.size());
				surface.numTriangles = int32_t(mesh.tris.size());
				out.write(surface);

				surface.ofsTriangles = out.tell() - start;
				for (const auto &tri : mesh.tris)
				{
					out.write(skelTriangle_t{{tri.x, tri.y, tri.z}});
				}

				std::set<int> usedBones;
				surface.ofsVerts = out.tell() - start;
				for (const auto &vert : mesh.vertices)
				{
					if (vert.weightCount <= 0)
					{
						throw mt_ex("Mesh " + mesh.name + " has a vertex without weights");
					}

					VertexHeader header{};
					glm::vec3 normal = fileNormal(model, mesh, vert);
					std::memcpy(header.normal, glm::value_ptr(normal), sizeof(header.normal));
					header.texCoords[0] = vert.tex.x;
					header.texCoords[1] = vert.tex.y;
					header.numWeights = vert.weightCount;
					out.write(header);

					for (int w = 0; w < vert.weightCount; w++)
					{
						const Weight &weight = mesh.weights.at(size_t(vert.startWeight + w));
						const int bone = skeleton.remap.at(size_t(weight.jointId));
						if (bone < 0)
						{
							throw mt_ex("Mesh " + mesh.name + " has a weight on a tag bone");
						}

						skelWeight_t entry{};
						entry.boneIndex = bone;
						entry.boneWeight = weight.bias;
						std::memcpy(entry.offset, glm::value_ptr(weight.pos), sizeof(entry.offset));
						usedBones.insert(weight.jointId);
						out.write(entry);
					}
				}

				surface.ofsCollapseMap = out.tell() - start;
				for (int index : mesh.collapseMap)
				{
					out.write(int32_t(index));
				}

				auto references = boneReferences(model, skeleton, usedBones, mesh.boneReferences);
				surface.numBoneReferences = int32_t(references.size());
				surface.ofsBoneReferences = out.tell() - start;
				out.writeArray(references);

				surface.ofsEnd = out.tell() - start;
				out.patch(start, surface);
			}
		}

		/**
		 * Tags written to an MDM: the bone tags and the bones used as tags, every tag sits on a written bone
		 */
		std::vector<BoneTag> mdmTags(const SkeletalModel &model, const detail::EtSkeleton &skeleton)
		{
			std::vector<BoneTag> tags = model.tags;
			for (size_t i = 0; i < model.joints.size(); i++)
			{
				if (skeleton.remap[i] < 0)
				{
					tags.push_back(boneTagFromJoint(model, int(i)));
				}
			}

			// A tag on a tag bone is moved to the tag bone's parent
			for (auto &tag : tags)
			{
				while (tag.parentBone >= 0 && skeleton.remap.at(size_t(tag.parentBone)) < 0)
				{
					BoneTag parent = boneTagFromJoint(model, tag.parentBone);
					tag.location = parent.location + parent.rotation * tag.location;
					tag.rotation = glm::normalize(parent.rotation * tag.rotation);
					tag.parentBone = parent.parentBone;
				}

				if (tag.parentBone < 0)
				{
					throw mt_ex("Tag " + tag.name + " is not on a bone");
				}
			}
			return tags;
		}
	}

	std::vector<byte> encodeMDM(const Model &source)
	{
		auto prepared = prepareForExport(source);
		auto *model = dynamic_cast<SkeletalModel *>(prepared.get());
		if (!model)
		{
			throw mt_ex("MDM needs a skeletal model");
		}

		// An MDM loaded without its MDX has no bones: the bone numbers are written as they are
		detail::EtSkeleton skeleton;
		if (model->joints.empty())
		{
			int maxBone = -1;
			for (const auto &mesh : model->meshes)
			{
				for (const auto &weight : mesh.weights)
				{
					maxBone = std::max(maxBone, weight.jointId);
				}
			}
			for (const auto &tag : model->tags)
			{
				maxBone = std::max(maxBone, tag.parentBone);
			}
			skeleton.remap.resize(size_t(maxBone + 1));
			std::iota(skeleton.remap.begin(), skeleton.remap.end(), 0);
		}
		else
		{
			skeleton = detail::etSkeleton(*model, true);
		}
		ensureCollapseMaps(*model);
		std::vector<BoneTag> tags = model->joints.empty() ? model->tags : mdmTags(*model, skeleton);

		BinaryWriter out;
		mdmHeader_t header{};
		header.ident = MDM_IDENT;
		header.version = MDM_VERSION;
		BinaryWriter::setStringTruncated(header.name, model->name);
		header.lodScale = model->lodScale;
		header.lodBias = model->lodBias;
		header.numSurfaces = int32_t(model->meshes.size());
		header.numTags = int32_t(tags.size());
		out.write(header);

		header.ofsSurfaces = out.tell();
		writeSurfaces<mdmVertexHeader_t>(out, *model, skeleton);

		header.ofsTags = out.tell();
		for (const auto &tag : tags)
		{
			const int32_t start = out.tell();

			mdmTag_t entry{};
			BinaryWriter::setString(entry.name, tag.name, "tag name");
			glm::mat3 axis = tag.axis();
			for (int i = 0; i < 3; i++)
			{
				std::memcpy(entry.axis[i], glm::value_ptr(axis[i]), sizeof(entry.axis[i]));
			}
			entry.boneIndex = skeleton.remap[tag.parentBone];
			std::memcpy(entry.offset, glm::value_ptr(tag.location), sizeof(entry.offset));
			out.write(entry);

			auto references = boneReferences(*model, skeleton, {tag.parentBone}, tag.boneReferences);
			entry.numBoneReferences = int32_t(references.size());
			entry.ofsBoneReferences = out.tell() - start;
			out.writeArray(references);

			entry.ofsEnd = out.tell() - start;
			out.patch(start, entry);
		}

		header.ofsEnd = out.tell();
		out.patch(0, header);
		return out.data;
	}

	void writeMDM(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeMDM(model));
	}

	std::vector<byte> encodeMDS(const Model &source)
	{
		auto prepared = prepareForExport(source);
		auto *model = dynamic_cast<SkeletalModel *>(prepared.get());
		if (!model)
		{
			throw mt_ex("MDS needs a skeletal model");
		}

		// MDS tags are bones: a tag that sits on a bone with an offset becomes a new tag bone
		struct MdsTag {
			std::string name;
			int bone;
			float torsoWeight;
		};
		std::vector<MdsTag> tags;
		std::vector<bool> tagged(model->joints.size(), false);
		const int frames = detail::etFrameCount(*model);

		for (const auto &tag : model->tags)
		{
			const bool onBone = glm::length(tag.location) < 1e-4f && std::abs(std::abs(tag.rotation.w) - 1.0f) < 1e-5f;
			if (onBone)
			{
				tags.push_back({tag.name, tag.parentBone, tag.torsoWeight});
				tagged[size_t(tag.parentBone)] = true;
				continue;
			}

			Joint bone;
			bone.name = tag.name;
			bone.parentId = tag.parentBone;
			bone.asTag = true;
			bone.torsoWeight = tag.torsoWeight;
			const Joint &parent = model->joints.at(size_t(tag.parentBone));
			static_cast<Point &>(bone) = {parent.location + parent.rotation * tag.location, glm::normalize(parent.rotation * tag.rotation)};
			for (int frame = 0; frame < model->numFrames(); frame++)
			{
				const Point &pose = parent.frames.at(size_t(frame));
				bone.frames.push_back({pose.location + pose.rotation * tag.location, glm::normalize(pose.rotation * tag.rotation)});
			}

			tags.push_back({tag.name, int(model->joints.size()), tag.torsoWeight});
			model->joints.push_back(bone);
			tagged.push_back(true);
		}

		for (size_t i = 0; i < model->joints.size(); i++)
		{
			if (model->joints[i].asTag && !tagged[i])
			{
				tags.push_back({model->joints[i].name, int(i), model->joints[i].torsoWeight});
			}
		}

		detail::EtSkeleton skeleton = detail::etSkeleton(*model, false);
		ensureCollapseMaps(*model);
		detail::ensureBounds(*model);

		BinaryWriter out;
		mdsHeader_t header{};
		header.ident = MDS_IDENT;
		header.version = MDS_VERSION;
		BinaryWriter::setStringTruncated(header.name, model->name);
		header.lodScale = model->lodScale;
		header.lodBias = model->lodBias;
		header.numFrames = frames;
		header.numBones = int32_t(skeleton.bones.size());
		header.torsoParent = model->torsoParent;
		header.numSurfaces = int32_t(model->meshes.size());
		header.numTags = int32_t(tags.size());
		out.write(header);

		header.ofsFrames = out.tell();
		detail::writeBoneFrames(out, *model, skeleton);

		header.ofsBones = out.tell();
		detail::writeBoneInfos(out, *model, skeleton);

		header.ofsSurfaces = out.tell();
		writeSurfaces<mdsVertexHeader_t>(out, *model, skeleton);

		header.ofsTags = out.tell();
		for (const auto &tag : tags)
		{
			mdsTag_t entry{};
			BinaryWriter::setString(entry.name, tag.name, "tag name");
			entry.torsoWeight = tag.torsoWeight;
			entry.boneIndex = tag.bone;
			out.write(entry);
		}

		header.ofsEnd = out.tell();
		out.patch(0, header);
		return out.data;
	}

	void writeMDS(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeMDS(model));
	}
}
