#include "model_md3.h"
#include "binary_reader.h"
#include "idtech.h"

namespace mt::model {
	using namespace mt::idtech;
	using mt::io::BinaryReader;

	static_assert(sizeof(md3Header_t) == 108);
	static_assert(sizeof(md3Frame_t) == 56);
	static_assert(sizeof(md3Tag_t) == 112);
	static_assert(sizeof(md3Surface_t) == 108);
	static_assert(sizeof(md3XyzNormal_t) == 8);
	static_assert(sizeof(mdcHeader_t) == 112);
	static_assert(sizeof(mdcSurface_t) == 124);
	static_assert(sizeof(mdcTag_t) == 12);

	/*! Parsed file, frames and tags are only used for the base level of detail */
	struct MorphFile {
		std::string name;
		std::vector<morph::Frame> frames;
		std::vector<morph::Tag> tags;
		std::vector<morph::Mesh> meshes;
	};

	static morph::Frame readFrame(const md3Frame_t &frame)
	{
		morph::Frame out;
		out.name = BinaryReader::toString(frame.name);
		out.bounds = {toVec3(frame.bounds[0]), toVec3(frame.bounds[1])};
		out.localOrigin = toVec3(frame.localOrigin);
		out.radius = frame.radius;
		return out;
	}

	static std::string firstShader(BinaryReader &surface, int32_t offset, int32_t count)
	{
		surface.seek(offset);
		auto shaders = surface.readArray<md3Shader_t>(count);
		return shaders.empty() ? "" : BinaryReader::toString(shaders[0].name);
	}

	static std::vector<glm::vec2> readTexCoords(BinaryReader &surface, int32_t offset, int32_t count)
	{
		surface.seek(offset);
		auto st = surface.readArray<md3St_t>(count);

		std::vector<glm::vec2> out(st.size());
		std::transform(st.begin(), st.end(), out.begin(), [](const md3St_t &value) { return glm::vec2{value.st[0], value.st[1]}; });
		return out;
	}

	static std::vector<glm::ivec3> readTriangles(BinaryReader &surface, int32_t offset, int32_t count)
	{
		surface.seek(offset);
		auto tris = surface.readArray<md3Triangle_t>(count);

		std::vector<glm::ivec3> out(tris.size());
		std::transform(tris.begin(), tris.end(), out.begin(),
					   [](const md3Triangle_t &tri) { return glm::ivec3{tri.indexes[0], tri.indexes[1], tri.indexes[2]}; });
		return out;
	}

	static morph::FrameVertex readVertex(const md3XyzNormal_t &vert)
	{
		return {glm::vec3{vert.xyz[0], vert.xyz[1], vert.xyz[2]} * MD3_XYZ_SCALE, latLngToNormal(vert.normal)};
	}

	static int64_t nextSurface(int64_t offset, int32_t ofsEnd)
	{
		if (ofsEnd <= 0)
		{
			throw mt_ex("Invalid surface end offset: " + std::to_string(ofsEnd));
		}
		return offset + ofsEnd;
	}

	static MorphFile parseMD3(const Ref<mt::io::MTFile> &file)
	{
		BinaryReader reader(file);
		auto header = reader.read<md3Header_t>();

		if (header.ident != MD3_IDENT)
		{
			throw mt_ex("Not an MD3 file");
		}

		if (header.version != MD3_VERSION)
		{
			throw mt_ex("Unsupported MD3 version: " + std::to_string(header.version));
		}

		MorphFile out;
		out.name = BinaryReader::toString(header.name);

		reader.seek(header.ofsFrames);
		for (const auto &frame : reader.readArray<md3Frame_t>(header.numFrames))
		{
			out.frames.push_back(readFrame(frame));
		}

		reader.seek(header.ofsTags);
		auto tags = reader.readArray<md3Tag_t>(int64_t(header.numFrames) * header.numTags);
		out.tags.resize(size_t(header.numTags));
		for (int32_t i = 0; i < header.numTags; i++)
		{
			out.tags[i].name = BinaryReader::toString(tags[i].name);
			for (int32_t frame = 0; frame < header.numFrames; frame++)
			{
				const auto &tag = tags[size_t(frame) * header.numTags + i];
				out.tags[i].frames.push_back(Point::fromAxis(toVec3(tag.origin), toAxis(tag.axis)));
			}
		}

		int64_t offset = header.ofsSurfaces;
		for (int32_t i = 0; i < header.numSurfaces; i++)
		{
			auto info = reader.readAt<md3Surface_t>(offset);
			BinaryReader surface = reader.from(offset);

			// Some exporters leave the surface frame count at 0, the game and LegacyModelView use the model's frame count
			if (info.numFrames != header.numFrames && info.numFrames != 0)
			{
				throw mt_ex("Surface frame count does not match the model: " + std::to_string(info.numFrames));
			}
			info.numFrames = header.numFrames;

			morph::Mesh mesh;
			mesh.name = BinaryReader::toString(info.name);
			mesh.flags = info.flags;
			mesh.materialName = firstShader(surface, info.ofsShaders, info.numShaders);
			mesh.texCoords = readTexCoords(surface, info.ofsSt, info.numVerts);
			mesh.tris = readTriangles(surface, info.ofsTriangles, info.numTriangles);

			surface.seek(info.ofsXyzNormals);
			auto xyz = surface.readArray<md3XyzNormal_t>(int64_t(info.numVerts) * info.numFrames);
			for (int32_t frame = 0; frame < info.numFrames; frame++)
			{
				auto start = xyz.begin() + int64_t(frame) * info.numVerts;
				std::vector<morph::FrameVertex> vertices(size_t(info.numVerts));
				std::transform(start, start + info.numVerts, vertices.begin(), readVertex);
				mesh.frames.push_back(std::move(vertices));
			}

			out.meshes.push_back(std::move(mesh));
			offset = nextSurface(offset, info.ofsEnd);
		}

		return out;
	}

	static MorphFile parseMDC(const Ref<mt::io::MTFile> &file)
	{
		BinaryReader reader(file);
		auto header = reader.read<mdcHeader_t>();

		if (header.ident != MDC_IDENT)
		{
			throw mt_ex("Not an MDC file");
		}

		if (header.version != MDC_VERSION)
		{
			throw mt_ex("Unsupported MDC version: " + std::to_string(header.version));
		}

		MorphFile out;
		out.name = BinaryReader::toString(header.name);

		// MDC frames are stored like the MD3 frames
		reader.seek(header.ofsFrames);
		for (const auto &frame : reader.readArray<md3Frame_t>(header.numFrames))
		{
			out.frames.push_back(readFrame(frame));
		}

		reader.seek(header.ofsTagNames);
		auto tagNames = reader.readArray<mdcTagName_t>(header.numTags);
		reader.seek(header.ofsTags);
		auto tags = reader.readArray<mdcTag_t>(int64_t(header.numFrames) * header.numTags);
		out.tags.resize(size_t(header.numTags));
		for (int32_t i = 0; i < header.numTags; i++)
		{
			out.tags[i].name = BinaryReader::toString(tagNames[i].name);
			for (int32_t frame = 0; frame < header.numFrames; frame++)
			{
				const auto &tag = tags[size_t(frame) * header.numTags + i];
				glm::vec3 origin = glm::vec3{tag.xyz[0], tag.xyz[1], tag.xyz[2]} * MD3_XYZ_SCALE;
				glm::vec3 angles = glm::vec3{tag.angles[0], tag.angles[1], tag.angles[2]} * MDC_TAG_ANGLE_SCALE;
				out.tags[i].frames.push_back(Point::fromAxis(origin, anglesToAxis(angles)));
			}
		}

		int64_t offset = header.ofsSurfaces;
		for (int32_t i = 0; i < header.numSurfaces; i++)
		{
			auto info = reader.readAt<mdcSurface_t>(offset);
			BinaryReader surface = reader.from(offset);

			morph::Mesh mesh;
			mesh.name = BinaryReader::toString(info.name);
			mesh.flags = info.flags;
			mesh.materialName = firstShader(surface, info.ofsShaders, info.numShaders);
			mesh.texCoords = readTexCoords(surface, info.ofsSt, info.numVerts);
			mesh.tris = readTriangles(surface, info.ofsTriangles, info.numTriangles);

			surface.seek(info.ofsXyzNormals);
			auto baseVertices = surface.readArray<md3XyzNormal_t>(int64_t(info.numVerts) * info.numBaseFrames);
			surface.seek(info.ofsXyzCompressed);
			auto compVertices = surface.readArray<uint32_t>(int64_t(info.numVerts) * info.numCompFrames);
			surface.seek(info.ofsFrameBaseFrames);
			auto baseFrames = surface.readArray<int16_t>(header.numFrames);

			std::vector<int16_t> compFrames;
			if (info.numCompFrames > 0)
			{
				surface.seek(info.ofsFrameCompFrames);
				compFrames = surface.readArray<int16_t>(header.numFrames);
			}

			for (int32_t frame = 0; frame < header.numFrames; frame++)
			{
				int base = baseFrames[frame];
				if (base < 0 || base >= info.numBaseFrames)
				{
					throw mt_ex("Invalid MDC base frame: " + std::to_string(base));
				}

				int comp = compFrames.empty() ? -1 : compFrames[frame];
				if (comp >= info.numCompFrames)
				{
					throw mt_ex("Invalid MDC compressed frame: " + std::to_string(comp));
				}

				std::vector<morph::FrameVertex> vertices(size_t(info.numVerts));
				for (int32_t v = 0; v < info.numVerts; v++)
				{
					vertices[v] = readVertex(baseVertices[size_t(base) * info.numVerts + v]);

					// Compressed frames are stored as small offsets from the base frame. The compressed normal is not
					// decoded, the base frame normal is used instead.
					if (comp >= 0)
					{
						vertices[v].pos += decodeMdcOffset(compVertices[size_t(comp) * info.numVerts + v]);
					}
				}
				mesh.frames.push_back(std::move(vertices));
			}

			out.meshes.push_back(std::move(mesh));
			offset = nextSurface(offset, info.ofsEnd);
		}

		return out;
	}

	static MorphFile parseMorphFile(const Ref<mt::io::MTFile> &file)
	{
		const std::string ext = io::normalizeExtension(file->ext.empty() ? file->name : file->ext);
		if (ext == ".mdc")
		{
			return parseMDC(file);
		}
		return parseMD3(file);
	}

	static Model *toModel(MorphFile &&parsed)
	{
		auto model = std::make_unique<morph::MorphModel>();
		model->name = std::move(parsed.name);
		model->frames = std::move(parsed.frames);
		model->tags = std::move(parsed.tags);
		model->meshes = std::move(parsed.meshes);
		model->validate();
		return model.release();
	}

	Model *loadMD3(const Ref<mt::io::MTFile> &file)
	{
		return toModel(parseMD3(file));
	}

	Model *loadMDC(const Ref<mt::io::MTFile> &file)
	{
		return toModel(parseMDC(file));
	}

	void addMorphLod(morph::MorphModel &model, const Ref<mt::io::MTFile> &file)
	{
		if (!file)
		{
			throw mt_ex("File not found");
		}

		MorphFile parsed = parseMorphFile(file);

		if (parsed.frames.size() != model.frames.size())
		{
			throw mt_ex("Level of detail " + file->name + " has " + std::to_string(parsed.frames.size()) + " frames, the model has "
						+ std::to_string(model.frames.size()));
		}

		model.lods.push_back(std::move(parsed.meshes));

		try
		{
			model.validate();
		}
		catch (...)
		{
			model.lods.pop_back();
			throw;
		}
	}

	std::string morphLodPath(const std::string &path, int level)
	{
		auto dot = path.find_last_of('.');
		auto slash = path.find_last_of("/\\");

		if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		{
			return path + "_" + std::to_string(level);
		}

		return path.substr(0, dot) + "_" + std::to_string(level) + path.substr(dot);
	}
}
