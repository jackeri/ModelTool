#include "model_md3.h"
#include "binary_reader.h"
#include "binary_writer.h"
#include "model_export.h"

#include <cstring>
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

	std::vector<std::string> loadMorphLods(morph::MorphModel &model, mt::io::FileSource &source, const std::string &path)
	{
		std::vector<std::string> problems;
		for (int level = 1; level < 4; level++)
		{
			std::string lodPath = morphLodPath(path, level);
			if (!source.findFile(lodPath))
			{
				break;
			}

			try
			{
				addMorphLod(model, source.loadFile(lodPath));
			}
			catch (const std::exception &ex)
			{
				problems.push_back("Skipping level of detail " + lodPath + ": " + ex.what());
				break;
			}
		}
		return problems;
	}

	namespace {
		using mt::io::BinaryWriter;

		const int MD3_MAX_TRIANGLES = 8192;
		const int MD3_MAX_VERTS = 4096;
		const int MD3_MAX_SURFACES = 32;
		const int MD3_MAX_TAGS = 16;
		const int MD3_MAX_FRAMES = 1024;

		int16_t encodeCoordinate(float value)
		{
			const float scaled = std::round(value / MD3_XYZ_SCALE);
			if (scaled < float(INT16_MIN) || scaled > float(INT16_MAX))
			{
				throw mt_ex("A vertex is outside the MD3 range (+-512 units): " + std::to_string(value));
			}
			return int16_t(scaled);
		}

		std::unique_ptr<morph::MorphModel> morphForExport(const Model &model)
		{
			auto prepared = prepareForExport(model);
			if (auto *skeletal = dynamic_cast<skeletal::SkeletalModel *>(prepared.get()))
			{
				return toMorphModel(*skeletal);
			}
			return std::unique_ptr<morph::MorphModel>(dynamic_cast<morph::MorphModel *>(prepared.release()));
		}
	}

	std::vector<byte> encodeMD3(const Model &source, int level, bool tagsOnly)
	{
		auto model = morphForExport(source);
		const auto &meshes = model->lod(level);
		const int numFrames = model->numFrames();

		auto limit = [](const char *what, size_t count, int max) {
			if (int(count) > max)
			{
				throw mt_ex("The model has " + std::to_string(count) + " " + what + ", MD3 allows " + std::to_string(max)
							+ " (use the export frames or remove " + what + ")");
			}
		};
		limit("frames", size_t(numFrames), MD3_MAX_FRAMES);
		limit("tags", model->tags.size(), MD3_MAX_TAGS);
		if (!tagsOnly)
		{
			limit("meshes", meshes.size(), MD3_MAX_SURFACES);
		}

		BinaryWriter out;
		md3Header_t header{};
		header.ident = MD3_IDENT;
		header.version = MD3_VERSION;
		BinaryWriter::setStringTruncated(header.name, model->name);
		header.numFrames = numFrames;
		header.numTags = int32_t(model->tags.size());
		header.numSurfaces = tagsOnly ? 0 : int32_t(meshes.size());
		out.write(header);

		header.ofsFrames = out.tell();
		for (const auto &frame : model->frames)
		{
			md3Frame_t entry{};
			std::memcpy(entry.bounds[0], glm::value_ptr(frame.bounds.min), sizeof(entry.bounds[0]));
			std::memcpy(entry.bounds[1], glm::value_ptr(frame.bounds.max), sizeof(entry.bounds[1]));
			std::memcpy(entry.localOrigin, glm::value_ptr(frame.localOrigin), sizeof(entry.localOrigin));
			entry.radius = frame.radius;
			BinaryWriter::setStringTruncated(entry.name, frame.name);
			out.write(entry);
		}

		header.ofsTags = out.tell();
		for (int frame = 0; frame < numFrames; frame++)
		{
			for (const auto &tag : model->tags)
			{
				md3Tag_t entry{};
				BinaryWriter::setString(entry.name, tag.name, "tag name");
				const Point &point = tag.frames.at(size_t(frame));
				std::memcpy(entry.origin, glm::value_ptr(point.location), sizeof(entry.origin));
				glm::mat3 axis = point.axis();
				for (int i = 0; i < 3; i++)
				{
					std::memcpy(entry.axis[i], glm::value_ptr(axis[i]), sizeof(entry.axis[i]));
				}
				out.write(entry);
			}
		}

		header.ofsSurfaces = out.tell();
		if (!tagsOnly)
		{
			for (const auto &mesh : meshes)
			{
				if (int(mesh.numVertices()) > MD3_MAX_VERTS || int(mesh.tris.size()) > MD3_MAX_TRIANGLES)
				{
					throw mt_ex("Mesh " + mesh.name + " does not fit in MD3: max " + std::to_string(MD3_MAX_VERTS) + " vertices and "
								+ std::to_string(MD3_MAX_TRIANGLES) + " triangles");
				}

				const int32_t start = out.tell();
				md3Surface_t surface{};
				surface.ident = MD3_IDENT;
				BinaryWriter::setString(surface.name, mesh.name, "mesh name");
				surface.flags = mesh.flags;
				surface.numFrames = numFrames;
				surface.numShaders = 1;
				surface.numVerts = int32_t(mesh.numVertices());
				surface.numTriangles = int32_t(mesh.tris.size());
				out.write(surface);

				surface.ofsTriangles = out.tell() - start;
				for (const auto &tri : mesh.tris)
				{
					out.write(md3Triangle_t{{tri.x, tri.y, tri.z}});
				}

				surface.ofsShaders = out.tell() - start;
				md3Shader_t shader{};
				BinaryWriter::setString(shader.name, mesh.materialName, "material name");
				out.write(shader);

				surface.ofsSt = out.tell() - start;
				for (const auto &tex : mesh.texCoords)
				{
					out.write(md3St_t{{tex.x, tex.y}});
				}

				surface.ofsXyzNormals = out.tell() - start;
				for (const auto &frame : mesh.frames)
				{
					for (const auto &vert : frame)
					{
						md3XyzNormal_t entry{};
						for (int i = 0; i < 3; i++)
						{
							entry.xyz[i] = encodeCoordinate(vert.pos[i]);
						}
						entry.normal = normalToLatLng(vert.normal);
						out.write(entry);
					}
				}

				surface.ofsEnd = out.tell() - start;
				out.patch(start, surface);
			}
		}

		header.ofsEnd = out.tell();
		out.patch(0, header);
		return out.data;
	}

	std::vector<byte> encodeMDC(const Model &source, int level, const MdcOptions &options)
	{
		auto model = morphForExport(source);
		const auto &meshes = model->lod(level);
		const int numFrames = model->numFrames();

		auto limit = [](const char *what, size_t count, int max) {
			if (int(count) > max)
			{
				throw mt_ex("The model has " + std::to_string(count) + " " + what + ", MDC allows " + std::to_string(max)
							+ " (use the export frames or remove " + what + ")");
			}
		};
		limit("frames", size_t(numFrames), MD3_MAX_FRAMES);
		limit("tags", model->tags.size(), MD3_MAX_TAGS);
		limit("meshes", meshes.size(), MD3_MAX_SURFACES);

		BinaryWriter out;
		mdcHeader_t header{};
		header.ident = MDC_IDENT;
		header.version = MDC_VERSION;
		BinaryWriter::setStringTruncated(header.name, model->name);
		header.numFrames = numFrames;
		header.numTags = int32_t(model->tags.size());
		header.numSurfaces = int32_t(meshes.size());
		out.write(header);

		header.ofsFrames = out.tell();
		for (const auto &frame : model->frames)
		{
			md3Frame_t entry{};
			std::memcpy(entry.bounds[0], glm::value_ptr(frame.bounds.min), sizeof(entry.bounds[0]));
			std::memcpy(entry.bounds[1], glm::value_ptr(frame.bounds.max), sizeof(entry.bounds[1]));
			std::memcpy(entry.localOrigin, glm::value_ptr(frame.localOrigin), sizeof(entry.localOrigin));
			entry.radius = frame.radius;
			BinaryWriter::setStringTruncated(entry.name, frame.name);
			out.write(entry);
		}

		// Tags: names once, then location and angles per frame
		header.ofsTagNames = out.tell();
		for (const auto &tag : model->tags)
		{
			mdcTagName_t entry{};
			BinaryWriter::setString(entry.name, tag.name, "tag name");
			out.write(entry);
		}
		header.ofsTags = out.tell();
		for (int frame = 0; frame < numFrames; frame++)
		{
			for (const auto &tag : model->tags)
			{
				const Point &point = tag.frames.at(size_t(frame));
				const glm::vec3 angles = axisToAngles(point.axis());
				mdcTag_t entry{};
				for (int i = 0; i < 3; i++)
				{
					entry.xyz[i] = encodeCoordinate(point.location[i]);
					entry.angles[i] = int16_t(std::lround(angles[i] / MDC_TAG_ANGLE_SCALE));
				}
				out.write(entry);
			}
		}

		header.ofsSurfaces = out.tell();
		for (const auto &mesh : meshes)
		{
			if (int(mesh.numVertices()) > MD3_MAX_VERTS || int(mesh.tris.size()) > MD3_MAX_TRIANGLES)
			{
				throw mt_ex("Mesh " + mesh.name + " does not fit in MDC: max " + std::to_string(MD3_MAX_VERTS) + " vertices and "
							+ std::to_string(MD3_MAX_TRIANGLES) + " triangles");
			}
			const size_t numVerts = mesh.numVertices();

			// Full frames, and the frames stored as offsets from the last full frame when they fit
			std::vector<int> fullFrames, compressedFrames;
			std::vector<int16_t> frameBase(static_cast<size_t>(numFrames), 0);
			std::vector<int16_t> frameCompressed(static_cast<size_t>(numFrames), -1);
			auto quantized = [](const glm::vec3 &pos) {
				return glm::vec3{encodeCoordinate(pos.x), encodeCoordinate(pos.y), encodeCoordinate(pos.z)} * MD3_XYZ_SCALE;
			};
			auto offsetCode = [](float offset) { return std::lround(offset / MDC_DIST_SCALE + MDC_MAX_OFS); };
			for (int frame = 0; frame < numFrames; frame++)
			{
				bool fits = options.compress && !fullFrames.empty();
				for (size_t v = 0; fits && v < numVerts; v++)
				{
					const glm::vec3 offset = mesh.frames[size_t(frame)][v].pos - quantized(mesh.frames[size_t(fullFrames.back())][v].pos);
					for (int i = 0; i < 3; i++)
					{
						const long code = offsetCode(offset[i]);
						fits = fits && code >= 0 && code <= 255;
					}
				}
				if (fits)
				{
					frameBase[size_t(frame)] = int16_t(fullFrames.size() - 1);
					frameCompressed[size_t(frame)] = int16_t(compressedFrames.size());
					compressedFrames.push_back(frame);
				}
				else
				{
					frameBase[size_t(frame)] = int16_t(fullFrames.size());
					fullFrames.push_back(frame);
				}
			}

			const int32_t start = out.tell();
			mdcSurface_t surface{};
			surface.ident = MDC_IDENT;
			BinaryWriter::setString(surface.name, mesh.name, "mesh name");
			surface.flags = mesh.flags;
			surface.numCompFrames = int32_t(compressedFrames.size());
			surface.numBaseFrames = int32_t(fullFrames.size());
			surface.numShaders = 1;
			surface.numVerts = int32_t(numVerts);
			surface.numTriangles = int32_t(mesh.tris.size());
			out.write(surface);

			surface.ofsTriangles = out.tell() - start;
			for (const auto &tri : mesh.tris)
			{
				out.write(md3Triangle_t{{tri.x, tri.y, tri.z}});
			}

			surface.ofsShaders = out.tell() - start;
			md3Shader_t shader{};
			BinaryWriter::setString(shader.name, mesh.materialName, "material name");
			out.write(shader);

			surface.ofsSt = out.tell() - start;
			for (const auto &tex : mesh.texCoords)
			{
				out.write(md3St_t{{tex.x, tex.y}});
			}

			surface.ofsXyzNormals = out.tell() - start;
			for (int frame : fullFrames)
			{
				for (const auto &vert : mesh.frames[size_t(frame)])
				{
					md3XyzNormal_t entry{};
					for (int i = 0; i < 3; i++)
					{
						entry.xyz[i] = encodeCoordinate(vert.pos[i]);
					}
					entry.normal = normalToLatLng(vert.normal);
					out.write(entry);
				}
			}

			surface.ofsXyzCompressed = out.tell() - start;
			for (int frame : compressedFrames)
			{
				const auto &base = mesh.frames[size_t(fullFrames[size_t(frameBase[size_t(frame)])])];
				for (size_t v = 0; v < numVerts; v++)
				{
					const glm::vec3 offset = mesh.frames[size_t(frame)][v].pos - quantized(base[v].pos);
					uint32_t packed = 0;
					for (int i = 0; i < 3; i++)
					{
						packed |= uint32_t(offsetCode(offset[i])) << (8 * i);
					}
					out.write(packed); // the normal index (top 8 bits) is left at 0
				}
			}

			surface.ofsFrameBaseFrames = out.tell() - start;
			for (int16_t base : frameBase)
			{
				out.write(base);
			}
			surface.ofsFrameCompFrames = out.tell() - start;
			for (int16_t compressed : frameCompressed)
			{
				out.write(compressed);
			}

			surface.ofsEnd = out.tell() - start;
			out.patch(start, surface);
		}

		header.ofsEnd = out.tell();
		out.patch(0, header);
		return out.data;
	}

	void writeMDC(const Model &model, const std::string &path, const MdcOptions &options)
	{
		int levels = 1;
		if (const auto *morph = dynamic_cast<const morph::MorphModel *>(&model))
		{
			levels = morph->lodCount();
		}

		std::vector<std::vector<byte>> files;
		for (int level = 0; level < levels; level++)
		{
			files.push_back(encodeMDC(model, level, options));
		}
		for (int level = 0; level < levels; level++)
		{
			mt::io::saveFile(level == 0 ? path : morphLodPath(path, level), files[level]);
		}
	}

	void writeMD3(const Model &model, const std::string &path)
	{
		int levels = 1;
		if (const auto *morph = dynamic_cast<const morph::MorphModel *>(&model))
		{
			levels = morph->lodCount();
		}

		// Encode every level first so nothing is written when a level fails
		std::vector<std::vector<byte>> files;
		for (int level = 0; level < levels; level++)
		{
			files.push_back(encodeMD3(model, level));
		}

		for (int level = 0; level < levels; level++)
		{
			mt::io::saveFile(level == 0 ? path : morphLodPath(path, level), files[level]);
		}
	}

	void writeMD3Tags(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeMD3(model, 0, true));
	}
}
