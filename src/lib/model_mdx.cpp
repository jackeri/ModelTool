#include "model_mdx.h"
#include "binary_reader.h"
#include "binary_writer.h"
#include "model_export.h"
#include "skeletal_tools.h"
#include "idtech.h"

namespace mt::model {
	using namespace mt::idtech;
	using namespace mt::model::skeletal;
	using mt::io::BinaryReader;

	static_assert(sizeof(mdxHeader_t) == 96);
	static_assert(sizeof(mdxFrame_t) == 52);
	static_assert(sizeof(skelBoneFrameCompressed_t) == 12);
	static_assert(sizeof(skelBoneInfo_t) == 80);

	namespace detail {
		void createJoints(SkeletalModel &model, const std::vector<skelBoneInfo_t> &bones)
		{
			for (size_t i = 0; i < bones.size(); i++)
			{
				const auto &info = bones[i];

				// The frames are calculated in order, so the parents need to come first
				if (info.parent >= int32_t(i))
				{
					throw mt_ex("Bone " + std::to_string(i) + " has an invalid parent: " + std::to_string(info.parent));
				}

				Joint joint;
				joint.name = BinaryReader::toString(info.name);
				joint.parentId = info.parent < 0 ? -1 : info.parent;
				joint.torsoWeight = info.torsoWeight;
				joint.parentDist = info.parentDist;
				joint.asTag = (info.flags & BONEFLAG_TAG) != 0;
				model.joints.push_back(joint);
			}
		}

		void readBoneFrames(const BinaryReader &reader, int64_t ofsFrames, int32_t numFrames, const std::vector<skelBoneInfo_t> &bones,
							SkeletalModel &model)
		{
			if (model.joints.size() != bones.size())
			{
				throw mt_ex("Joint count does not match the bone count");
			}

			const int64_t numBones = int64_t(bones.size());
			const int64_t frameSize = int64_t(sizeof(mdxFrame_t)) + numBones * int64_t(sizeof(skelBoneFrameCompressed_t));

			if (numFrames < 0)
			{
				throw mt_ex("Invalid frame count: " + std::to_string(numFrames));
			}

			if (!reader.canRead(ofsFrames, frameSize * numFrames))
			{
				throw mt_ex("Frame data does not fit in the file");
			}

			for (auto &joint : model.joints)
			{
				joint.frames.clear();
				joint.frames.reserve(size_t(numFrames));
			}
			model.bounds.clear();

			BinaryReader frames = reader.sub(ofsFrames, frameSize * numFrames);
			for (int32_t i = 0; i < numFrames; i++)
			{
				auto frame = frames.read<mdxFrame_t>();
				auto compressed = frames.readArray<skelBoneFrameCompressed_t>(numBones);

				// Files are not consistent about which corner comes first
				glm::vec3 corner1 = toVec3(frame.bounds[0]), corner2 = toVec3(frame.bounds[1]);
				model.bounds.push_back({glm::min(corner1, corner2), glm::max(corner1, corner2)});

				for (int64_t b = 0; b < numBones; b++)
				{
					const auto &bone = compressed[b];
					const auto &info = bones[b];
					Joint &joint = model.joints[b];

					glm::vec3 angles{shortToAngle(bone.angles[0]), shortToAngle(bone.angles[1]), shortToAngle(bone.angles[2])};

					Point point;
					point.rotation = boneRotation(anglesToAxis(angles));

					if (info.parent >= 0)
					{
						// Child bones are stored as a direction and a distance from the parent
						const Point &parent = model.joints[info.parent].frames.back();
						point.location = parent.location + info.parentDist * ingleVector(bone.ofsAngles[0], bone.ofsAngles[1]);
					}
					else
					{
						point.location = toVec3(frame.parentOffset);
					}

					joint.frames.push_back(point);
				}
			}
		}

		EtSkeleton etSkeleton(const SkeletalModel &model, bool tagBonesAsTags)
		{
			EtSkeleton out;
			out.remap.assign(model.joints.size(), -1);
			const std::vector<bool> tags = tagBonesAsTags ? tagBones(model) : std::vector<bool>(model.joints.size(), false);

			int roots = 0;
			for (size_t i = 0; i < model.joints.size(); i++)
			{
				if (tags[i])
				{
					continue;
				}
				out.remap[i] = int(out.bones.size());
				out.bones.push_back(int(i));
				roots += model.joints[i].parentId < 0 ? 1 : 0;
			}

			if (out.bones.empty())
			{
				throw mt_ex("The model has no bones");
			}

			// Only one bone position is stored per frame (the parent offset), the others are relative to their parents
			if (roots > 1)
			{
				throw mt_ex("The skeleton has " + std::to_string(roots)
							+ " root bones, the ET formats support one (Edit > Check armature)");
			}

			return out;
		}

		int etFrameCount(const SkeletalModel &model)
		{
			return std::max(model.numFrames(), 1);
		}

		const Point &etFramePose(const SkeletalModel &model, int bone, int frame)
		{
			const Joint &joint = model.joints.at(size_t(bone));
			return joint.frames.empty() ? static_cast<const Point &>(joint) : joint.frames.at(size_t(frame));
		}

		void ensureBounds(SkeletalModel &model)
		{
			const int frames = etFrameCount(model);
			if (int(model.bounds.size()) == frames)
			{
				return;
			}

			if (model.numFrames() > 0 && !model.meshes.empty())
			{
				calculateBounds(model);
				return;
			}

			// Bind pose or no meshes: bounds around the vertices or the bones
			model.bounds.assign(size_t(frames), Bounds{});
			for (int frame = 0; frame < frames; frame++)
			{
				bool any = false;
				auto add = [&](const glm::vec3 &pos) {
					model.bounds[frame].min = any ? glm::min(model.bounds[frame].min, pos) : pos;
					model.bounds[frame].max = any ? glm::max(model.bounds[frame].max, pos) : pos;
					any = true;
				};
				for (const auto &mesh : model.meshes)
				{
					for (const auto &vert : mesh.vertices)
					{
						add(posedVertex(model, mesh, vert, model.numFrames() > 0 ? frame : FRAME_BIND_POSE));
					}
				}
				for (size_t b = 0; b < model.joints.size(); b++)
				{
					add(etFramePose(model, int(b), frame).location);
				}
			}
		}

		void writeBoneFrames(mt::io::BinaryWriter &out, const SkeletalModel &model, const EtSkeleton &skeleton)
		{
			const int frames = etFrameCount(model);
			const int root
				= *std::find_if(skeleton.bones.begin(), skeleton.bones.end(), [&](int b) { return model.joints[b].parentId < 0; });

			for (int frame = 0; frame < frames; frame++)
			{
				const Bounds &bounds = model.bounds.at(size_t(frame));

				mdxFrame_t header{};
				for (int i = 0; i < 3; i++)
				{
					header.bounds[0][i] = bounds.min[i];
					header.bounds[1][i] = bounds.max[i];
				}
				glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
				std::memcpy(header.localOrigin, glm::value_ptr(center), sizeof(header.localOrigin));
				header.radius = glm::distance(center, bounds.max);
				const glm::vec3 rootLocation = etFramePose(model, root, frame).location;
				std::memcpy(header.parentOffset, glm::value_ptr(rootLocation), sizeof(header.parentOffset));
				out.write(header);

				for (int bone : skeleton.bones)
				{
					const Point &pose = etFramePose(model, bone, frame);
					glm::vec3 angles = axisToAngles(boneAxis(pose.rotation));

					skelBoneFrameCompressed_t compressed{};
					compressed.angles[0] = angleToShort(angles[PITCH]);
					compressed.angles[1] = angleToShort(angles[YAW]);
					compressed.angles[2] = angleToShort(angles[ROLL]);

					const int parent = model.joints[bone].parentId;
					if (parent >= 0)
					{
						glm::ivec2 ingles = vectorToIngles(pose.location - etFramePose(model, parent, frame).location);
						compressed.ofsAngles[0] = int16_t(ingles.x);
						compressed.ofsAngles[1] = int16_t(ingles.y);
					}

					out.write(compressed);
				}
			}
		}

		void writeBoneInfos(mt::io::BinaryWriter &out, const SkeletalModel &model, const EtSkeleton &skeleton)
		{
			for (int bone : skeleton.bones)
			{
				const Joint &joint = model.joints[bone];

				skelBoneInfo_t info{};
				mt::io::BinaryWriter::setString(info.name, joint.name, "bone name");
				info.parent = joint.parentId >= 0 ? skeleton.remap[joint.parentId] : -1;
				info.torsoWeight = joint.torsoWeight;
				info.flags = joint.asTag ? BONEFLAG_TAG : 0;

				// The bone length is stored once, measured in the first frame
				if (joint.parentId >= 0)
				{
					info.parentDist = glm::distance(etFramePose(model, bone, 0).location, etFramePose(model, joint.parentId, 0).location);
				}

				out.write(info);
			}
		}

		void validateWeights(const SkeletalModel &model)
		{
			for (const auto &mesh : model.meshes)
			{
				for (const auto &weight : mesh.weights)
				{
					if (weight.jointId < 0 || weight.jointId >= int(model.joints.size()))
					{
						throw mt_ex("Mesh " + mesh.name + " has a weight for an invalid bone: " + std::to_string(weight.jointId));
					}
				}
			}

			for (const auto &tag : model.tags)
			{
				if (tag.parentBone < 0 || tag.parentBone >= int(model.joints.size()))
				{
					throw mt_ex("Tag " + tag.name + " has an invalid bone: " + std::to_string(tag.parentBone));
				}
			}
		}
	}

	static void readMDX(const Ref<mt::io::MTFile> &file, SkeletalModel &model)
	{
		BinaryReader reader(file);
		auto header = reader.read<mdxHeader_t>();

		if (header.ident != MDX_IDENT)
		{
			throw mt_ex("Not an MDX file");
		}

		if (header.version != MDX_VERSION)
		{
			throw mt_ex("Unsupported MDX version: " + std::to_string(header.version));
		}

		reader.seek(header.ofsBones);
		auto bones = reader.readArray<skelBoneInfo_t>(header.numBones);

		if (header.torsoParent < -1 || header.torsoParent >= header.numBones)
		{
			throw mt_ex("Invalid torso parent: " + std::to_string(header.torsoParent));
		}

		if (model.joints.empty())
		{
			detail::createJoints(model, bones);
		}
		else
		{
			if (model.joints.size() != bones.size())
			{
				throw mt_ex("The animation has " + std::to_string(bones.size()) + " bones, the model has "
							+ std::to_string(model.joints.size()));
			}

			for (size_t i = 0; i < bones.size(); i++)
			{
				if (model.joints[i].name != BinaryReader::toString(bones[i].name))
				{
					throw mt_ex("Bone names do not match: " + model.joints[i].name + " != " + BinaryReader::toString(bones[i].name));
				}
			}
		}

		detail::readBoneFrames(reader, header.ofsFrames, header.numFrames, bones, model);
		detail::validateWeights(model);

		model.torsoParent = header.torsoParent;

		// Without a bind pose there is nothing to show at the bind pose "frame"
		if (!model.hasBindPose && model.currentFrame < 0 && model.hasFrames())
		{
			model.currentFrame = 0;
		}
	}

	Model *loadMDX(const Ref<mt::io::MTFile> &file)
	{
		auto model = std::make_unique<SkeletalModel>();
		readMDX(file, *model);

		if (model->name.empty())
		{
			model->name = BinaryReader::toString(BinaryReader(file).read<mdxHeader_t>().name);
		}

		return model.release();
	}

	void loadMDXAnimation(Model *parent, const Ref<mt::io::MTFile> &file)
	{
		auto *model = dynamic_cast<SkeletalModel *>(parent);
		if (!model)
		{
			throw mt_ex("MDX animations can only be loaded into a skeletal model");
		}

		// Do not leave the model half updated if the file is broken
		SkeletalModel backup;
		backup.joints = model->joints;
		backup.bounds = model->bounds;
		int torsoParent = model->torsoParent;
		int currentFrame = model->currentFrame;

		try
		{
			readMDX(file, *model);
		}
		catch (...)
		{
			model->joints = std::move(backup.joints);
			model->bounds = std::move(backup.bounds);
			model->torsoParent = torsoParent;
			model->currentFrame = currentFrame;
			throw;
		}
	}

	std::vector<byte> encodeMDX(const Model &source)
	{
		auto prepared = prepareForExport(source);
		auto *model = dynamic_cast<SkeletalModel *>(prepared.get());
		if (!model)
		{
			throw mt_ex("MDX needs a skeletal model");
		}

		detail::EtSkeleton skeleton = detail::etSkeleton(*model, true);
		detail::ensureBounds(*model);

		mt::io::BinaryWriter out;
		mdxHeader_t header{};
		header.ident = MDX_IDENT;
		header.version = MDX_VERSION;
		mt::io::BinaryWriter::setStringTruncated(header.name, model->name);
		header.numFrames = detail::etFrameCount(*model);
		header.numBones = int32_t(skeleton.bones.size());
		header.torsoParent = model->torsoParent >= 0 ? skeleton.remap[model->torsoParent] : -1;
		out.write(header);

		header.ofsFrames = out.tell();
		detail::writeBoneFrames(out, *model, skeleton);

		header.ofsBones = out.tell();
		detail::writeBoneInfos(out, *model, skeleton);

		header.ofsEnd = out.tell();
		out.patch(0, header);
		return out.data;
	}

	void writeMDX(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeMDX(model));
	}
}
