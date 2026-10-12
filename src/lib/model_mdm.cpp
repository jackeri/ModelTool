#include "model_mdm.h"
#include "model_mdx.h"
#include "binary_reader.h"
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
}
