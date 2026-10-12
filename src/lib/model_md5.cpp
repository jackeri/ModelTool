#include "model_md5.h"
#include "skeletal_model.h"
#include "script_stream.h"
#include "normals.h"
#include "model_export.h"
#include "model_mdx.h"
#include "skeletal_tools.h"
#include "binary_writer.h"

#include <cstdarg>
#include <cstdio>

namespace mt::model {
	using namespace skeletal;

	// There should not be any other versions
	const int MD5_VERSION = 10;

	static void computeQuaternionWComponent(glm::quat &quat)
	{
		float comp = 1.0f - (quat.x * quat.x) - (quat.y * quat.y) - (quat.z * quat.z);
		if (comp < 0.0f)
		{
			quat.w = 0.0f;
		}
		else
		{
			quat.w = -std::sqrt(comp);
		}
	}

	static void parseMeshPart(ScriptStream &stream, SkeletalModel *model)
	{
		if (stream.token() != "{")
		{
			throw mt_ex("Invalid token: " + stream.last());
		}

		Mesh mesh;

		int numVerts = 0;
		int numTris = 0;
		int numWeights = 0;

		while (stream.token() != "}")
		{
			switch (hash(stream.last().c_str()))
			{
			case hash("shader"):
				// A mesh does not have a name per say, but usually the material name is the same as the mesh name
				mesh.name = mesh.materialName = stream.token();
				break;

			case hash("numverts"): numVerts = stream.parseInt(); break;

			case hash("vert"): {
				SkeletalVertex vertex;
				int offset = stream.parseInt();
				stream.parse1DMatrix(2, glm::value_ptr(vertex.tex));
				vertex.startWeight = stream.parseInt();
				vertex.weightCount = stream.parseInt();

				if (vertex.startWeight < 0 || vertex.weightCount <= 0)
				{
					throw mt_ex("Invalid vertex");
				}

				mesh.vertices.emplace_back(vertex);
				break;
			}

			case hash("numtris"): numTris = stream.parseInt(); break;

			case hash("tri"): {
				glm::ivec3 tri;

				int triangleOffset = stream.parseInt();

				if (triangleOffset != mesh.tris.size())
				{
					throw mt_ex("Invalid triangle offset: " + std::to_string(triangleOffset));
				}

				tri.x = stream.parseInt();
				tri.y = stream.parseInt();
				tri.z = stream.parseInt();

				mesh.tris.emplace_back(tri);
				break;
			}

			case hash("numweights"): numWeights = stream.parseInt(); break;

			case hash("weight"): {
				Weight weight;

				if (stream.parseInt() < 0)
				{
					throw mt_ex("Invalid weight offset");
				}

				weight.jointId = stream.parseInt();
				weight.bias = stream.parseFloat();
				stream.parse1DMatrix(3, glm::value_ptr(weight.pos));

				mesh.weights.emplace_back(weight);
				break;
			}

			default: {
				throw mt_ex("Unknown token: " + stream.last());
			}
			}
		}

		if (numVerts < 0 || numVerts != mesh.vertices.size())
		{
			throw mt_ex("Invalid amount of vertices: " + std::to_string(numVerts));
		}

		if (numTris < 0 || numTris != mesh.tris.size())
		{
			throw mt_ex("Invalid amount of triangles: " + std::to_string(numTris));
		}

		if (numWeights < 0 || numWeights != mesh.weights.size())
		{
			throw mt_ex("Invalid amount of weights: " + std::to_string(numWeights));
		}

		// Compute the bind pose vertex positions
		for (auto &vert : mesh.vertices)
		{
			vert.pos = {0, 0, 0};

			if (vert.startWeight + vert.weightCount > int(mesh.weights.size()))
			{
				throw mt_ex("Vertex points to an invalid weight");
			}

			// Sum the position of the weights
			for (int i = 0; i < vert.weightCount; i++)
			{
				auto &weight = mesh.weights[vert.startWeight + i];

				if (weight.jointId < 0 || weight.jointId >= int(model->joints.size()))
				{
					throw mt_ex("Weight points to an invalid joint: " + std::to_string(weight.jointId));
				}

				auto &joint = model->joints[weight.jointId];

				// Convert the weight positions from joint local space to object space
				glm::vec3 rotatedPosition = joint.rotation * weight.pos;
				vert.pos += (joint.location + rotatedPosition) * weight.bias;
			}
		}

		calculateNormals(*model, mesh, FRAME_BIND_POSE);

		model->meshes.emplace_back(mesh);
	}

	static void parseMeshRoot(ScriptStream &stream, SkeletalModel *model)
	{
		int numJoints = -1;
		int numMeshes = -1;

		// MD5 models do have a bind pose by default
		model->hasBindPose = true;

		while (stream.hasNext())
		{
			std::string token = stream.token();

			switch (hash(token.c_str()))
			{
			// Check that the version matches the one version that we actually support (this should always be the same)
			case hash("MD5Version"):
				if (stream.parseInt() != MD5_VERSION)
				{
					throw mt_ex("Invalid MD5 version: " + stream.last());
				}
				break;

				// This can be ignored
			case hash("commandline"):
				// Skip the rest of the line, we do not care about the command line used
				stream.skipLine();
				break;

			case hash("numJoints"): numJoints = (int)stream.parse(); break;

			case hash("numMeshes"): numMeshes = (int)stream.parse(); break;

			case hash("joints"): {
				if (stream.token() != "{")
				{
					throw mt_ex("Invalid joint data");
				}

				while (stream.token() != "}")
				{
					Joint joint;
					joint.name = stream.last();
					joint.parentId = stream.parseInt();
					stream.parse1DMatrix(3, glm::value_ptr(joint.location));
					stream.parse1DMatrix(3, glm::value_ptr(joint.rotation));
					computeQuaternionWComponent(joint.rotation);
					model->joints.push_back(joint);
				}
				break;
			}
			case hash("mesh"): {
				parseMeshPart(stream, model);
				break;
			}

			default: {
				throw mt_ex("Unknown token: " + token);
			}
			}
		}

		if (model->meshes.size() != numMeshes)
		{
			throw mt_ex("invalid number of meshes");
		}

		if (model->joints.size() != numJoints)
		{
			throw mt_ex("invalid number of joints");
		}
	}

	Model *loadMd5Model(const Ref<mt::io::MTFile> &file)
	{
		// MD5 model file is just a text file
		ScriptStream stream(file);
		auto model = std::make_unique<SkeletalModel>();

		parseMeshRoot(stream, model.get());

		return model.release();
	}

	static void parseAnimRoot(ScriptStream &stream, SkeletalModel *model)
	{
		int numFrames = -1;
		int frameRate = -1;
		int numAnimatedComponents = -1;

		int processedFrames = 0;
		std::vector<Point> baseFrame;

		/*! Hierarchy line of a joint: which components the frames animate and where they start */
		struct AnimatedJoint {
			int flags = 0;		///< 1, 2, 4 = position x, y, z; 8, 16, 32 = orientation x, y, z
			int startIndex = 0; ///< first value of the joint in a frame
		};
		std::vector<AnimatedJoint> hierarchy(model->joints.size());

		while (stream.hasNext())
		{
			std::string token = stream.token();

			switch (hash(token.c_str()))
			{
			// Check that the version matches the one version that we actually support (this should always be the same)
			case hash("MD5Version"):
				if ((int)stream.parse() != MD5_VERSION)
				{
					throw mt_ex("Invalid MD5 version: " + stream.last());
				}
				break;

				// This can be ignored
			case hash("commandline"):
				// Skip the rest of the line, we do not care about the command line used
				stream.skipLine();
				break;

			case hash("numFrames"): numFrames = stream.parseInt(); break;

			case hash("numJoints"):
				if (model->joints.size() != stream.parseInt())
				{
					throw mt_ex("Invalid number of joints expected: " + stream.last());
				}
				break;

			case hash("frameRate"): frameRate = stream.parseInt(); break;

			case hash("numAnimatedComponents"): numAnimatedComponents = stream.parseInt(); break;

			case hash("hierarchy"): {
				if (stream.token() != "{")
				{
					throw mt_ex("Invalid hierarchy data");
				}

				for (size_t jointIndex = 0; jointIndex < model->joints.size(); jointIndex++)
				{
					const auto &joint = model->joints[jointIndex];
					if (joint.name != stream.token())
					{
						throw mt_ex("Invalid joint hierarchy, name does not match: " + joint.name + " != " + stream.last());
					}

					if (joint.parentId != stream.parseInt())
					{
						throw mt_ex("Invalid joint hierarchy, parent ID does not match for: " + joint.name);
					}

					hierarchy[jointIndex].flags = stream.parseInt();
					hierarchy[jointIndex].startIndex = stream.parseInt();
				}

				if (stream.token() != "}")
				{
					throw mt_ex("Invalid number of joints");
				}

				break;
			}

			case hash("bounds"): {
				if (stream.token() != "{")
				{
					throw mt_ex("Invalid bounds data");
				}

				while (stream.peekNext() != "}")
				{
					Bounds bound{};
					stream.parse1DMatrix(3, glm::value_ptr(bound.min));
					stream.parse1DMatrix(3, glm::value_ptr(bound.max));
					model->bounds.emplace_back(bound);
				}
				stream.parse();
				break;
			}

			case hash("baseframe"): {
				// Values for the components that the frames do not animate (local to the parent joint)
				if (stream.token() != "{")
				{
					throw mt_ex("Invalid baseframe data");
				}

				baseFrame.clear();
				while (stream.peekNext() != "}")
				{
					Point point;
					stream.parse1DMatrix(3, glm::value_ptr(point.location));
					stream.parse1DMatrix(3, glm::value_ptr(point.rotation));
					baseFrame.push_back(point);
				}
				stream.parse();

				if (baseFrame.size() != model->joints.size())
				{
					throw mt_ex("The baseframe has " + std::to_string(baseFrame.size()) + " joints, the model has "
								+ std::to_string(model->joints.size()));
				}
				break;
			}

			case hash("frame"): {
				int frameNumber = stream.parseInt();
				std::vector<float> frameValues{};

				if (stream.token() != "{")
				{
					throw mt_ex("Invalid frame data for frame: " + std::to_string(frameNumber));
				}

				while (stream.peekNext() != "}")
				{
					frameValues.emplace_back(stream.parseFloat());
				}
				stream.parse();

				// calculate the final joint locations per frame
				for (size_t jointIndex = 0; jointIndex < model->joints.size(); jointIndex++)
				{
					auto &joint = model->joints[jointIndex];
					const AnimatedJoint &animated = hierarchy[jointIndex];

					// Components that are not animated keep their baseframe value
					Point point;
					if (jointIndex < baseFrame.size())
					{
						point = baseFrame[jointIndex];
					}

					unsigned int j = 0;

					if (animated.flags & 1) // Pos.x
					{
						point.location.x = frameValues.at(size_t(animated.startIndex) + j++);
					}
					if (animated.flags & 2) // Pos.y
					{
						point.location.y = frameValues.at(size_t(animated.startIndex) + j++);
					}
					if (animated.flags & 4) // Pos.z
					{
						point.location.z = frameValues.at(size_t(animated.startIndex) + j++);
					}
					if (animated.flags & 8) // Orient.x
					{
						point.rotation.x = frameValues.at(size_t(animated.startIndex) + j++);
					}
					if (animated.flags & 16) // Orient.y
					{
						point.rotation.y = frameValues.at(size_t(animated.startIndex) + j++);
					}
					if (animated.flags & 32) // Orient.z
					{
						point.rotation.z = frameValues.at(size_t(animated.startIndex) + j++);
					}

					computeQuaternionWComponent(point.rotation);

					// Has a parent joint so we need to calculate the position based on the parent position
					if (joint.parentId >= 0)
					{
						Point &parentJoint = model->joints[joint.parentId].frames.back();

						point.location = parentJoint.location + (parentJoint.rotation * point.location);
						point.rotation = glm::normalize(parentJoint.rotation * point.rotation);
					}

					joint.frames.push_back(point);
				}

				processedFrames++;

				break;
			}

			default: {
				throw mt_ex("Unknown token: " + token);
			}
			}
		}

		if (numFrames != processedFrames)
		{
			throw mt_ex("Invalid animation data. Framecounts do not match: " + std::to_string(numFrames)
						+ " != " + std::to_string(processedFrames));
		}
	}

	void loadMD5Anim(Model *baseModel, const Ref<mt::io::MTFile> &file)
	{
		auto *model = dynamic_cast<SkeletalModel *>(baseModel);
		if (!model)
		{
			throw mt_ex("MD5 animations can only be loaded into a skeletal model");
		}

		// MD5 model file is just a text file
		ScriptStream stream(file);

		parseAnimRoot(stream, model);
	}

	namespace {
		std::string printText(const char *fmt, ...)
		{
			char buffer[1024];
			va_list args;
			va_start(args, fmt);
			vsnprintf(buffer, sizeof(buffer), fmt, args);
			va_end(args);
			return buffer;
		}

		/*! MD5 stores x y z of unit quaternions with a negative (or zero) w */
		glm::quat md5Quaternion(glm::quat rotation)
		{
			rotation = glm::normalize(rotation);
			return rotation.w > 0 ? -rotation : rotation;
		}

		/*! Pose relative to the parent joint */
		Point localPose(const Point &pose, const Point *parent)
		{
			if (!parent)
			{
				return pose;
			}
			glm::quat inverse = glm::inverse(parent->rotation);
			return {inverse * (pose.location - parent->location), glm::normalize(inverse * pose.rotation)};
		}

		std::unique_ptr<SkeletalModel> skeletalForExport(const Model &model)
		{
			auto prepared = prepareForExport(model);
			auto *skeletal = dynamic_cast<SkeletalModel *>(prepared.get());
			if (!skeletal)
			{
				throw mt_ex("MD5 needs a skeletal model");
			}
			prepared.release();
			return std::unique_ptr<SkeletalModel>(skeletal);
		}

		std::string md5String(const std::string &value)
		{
			if (value.find('"') != std::string::npos)
			{
				throw mt_ex("Names can not contain quotes: " + value);
			}
			return "\"" + value + "\"";
		}
	}

	std::string encodeMD5Mesh(const Model &source)
	{
		auto model = skeletalForExport(source);
		if (!model->hasBindPose)
		{
			if (!model->hasFrames())
			{
				throw mt_ex("The model has no bind pose and no frames");
			}
			skeletal::bakeBindPose(*model, 0);
		}

		std::string out = "MD5Version 10\ncommandline \"Exported with ModelTool\"\n\n";
		out += printText("numJoints %zu\nnumMeshes %zu\n\njoints {\n", model->joints.size(), model->meshes.size());
		for (const auto &joint : model->joints)
		{
			glm::quat q = md5Quaternion(joint.rotation);
			out += "\t" + md5String(joint.name)
				   + printText("\t%d ( %.6f %.6f %.6f ) ( %.9f %.9f %.9f )\n", joint.parentId, joint.location.x, joint.location.y,
							   joint.location.z, q.x, q.y, q.z);
		}
		out += "}\n\n";

		for (const auto &mesh : model->meshes)
		{
			out += "mesh {\n\tshader " + md5String(mesh.materialName) + "\n\n";

			out += printText("\tnumverts %zu\n", mesh.vertices.size());
			for (size_t i = 0; i < mesh.vertices.size(); i++)
			{
				const auto &vert = mesh.vertices[i];
				out += printText("\tvert %zu ( %.6f %.6f ) %d %d\n", i, vert.tex.x, vert.tex.y, vert.startWeight, vert.weightCount);
			}

			out += printText("\n\tnumtris %zu\n", mesh.tris.size());
			for (size_t i = 0; i < mesh.tris.size(); i++)
			{
				out += printText("\ttri %zu %d %d %d\n", i, mesh.tris[i].x, mesh.tris[i].y, mesh.tris[i].z);
			}

			out += printText("\n\tnumweights %zu\n", mesh.weights.size());
			for (size_t i = 0; i < mesh.weights.size(); i++)
			{
				const auto &weight = mesh.weights[i];
				out += printText("\tweight %zu %d %.9f ( %.6f %.6f %.6f )\n", i, weight.jointId, weight.bias, weight.pos.x, weight.pos.y,
								 weight.pos.z);
			}
			out += "}\n\n";
		}

		return out;
	}

	std::string encodeMD5Anim(const Model &source)
	{
		auto model = skeletalForExport(source);
		if (!model->hasFrames())
		{
			throw mt_ex("The model has no frames");
		}

		detail::ensureBounds(*model);
		const int numFrames = model->numFrames();
		const size_t numJoints = model->joints.size();
		const int frameRate = model->animations.empty() ? 24 : std::max(model->animations.front().fps, 1);

		std::string out = "MD5Version 10\ncommandline \"Exported with ModelTool\"\n\n";
		out += printText("numFrames %d\nnumJoints %zu\nframeRate %d\nnumAnimatedComponents %zu\n\n", numFrames, numJoints, frameRate,
						 numJoints * 6);

		// Every joint has every component animated
		out += "hierarchy {\n";
		for (size_t i = 0; i < numJoints; i++)
		{
			out += "\t" + md5String(model->joints[i].name) + printText("\t%d 63 %zu\n", model->joints[i].parentId, i * 6);
		}
		out += "}\n\nbounds {\n";
		for (int frame = 0; frame < numFrames; frame++)
		{
			const Bounds &bounds = model->bounds[frame];
			out += printText("\t( %.6f %.6f %.6f ) ( %.6f %.6f %.6f )\n", bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x,
							 bounds.max.y, bounds.max.z);
		}

		auto jointLine = [&](int frame, size_t joint, bool baseframe) {
			const auto &info = model->joints[joint];
			const Point &pose = frame < 0 ? static_cast<const Point &>(info) : info.frames[frame];
			const Point *parent = nullptr;
			if (info.parentId >= 0)
			{
				const auto &parentJoint = model->joints[info.parentId];
				parent = frame < 0 ? &static_cast<const Point &>(parentJoint) : &parentJoint.frames[frame];
			}
			Point local = localPose(pose, parent);
			glm::quat q = md5Quaternion(local.rotation);
			return printText(baseframe ? "\t( %.6f %.6f %.6f ) ( %.9f %.9f %.9f )\n" : "\t%.6f %.6f %.6f %.9f %.9f %.9f\n",
							 local.location.x, local.location.y, local.location.z, q.x, q.y, q.z);
		};

		out += "}\n\nbaseframe {\n";
		for (size_t joint = 0; joint < numJoints; joint++)
		{
			out += jointLine(model->hasBindPose ? -1 : 0, joint, true);
		}
		out += "}\n\n";

		for (int frame = 0; frame < numFrames; frame++)
		{
			out += printText("frame %d {\n", frame);
			for (size_t joint = 0; joint < numJoints; joint++)
			{
				out += jointLine(frame, joint, false);
			}
			out += "}\n\n";
		}

		return out;
	}

	void writeMD5Mesh(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeMD5Mesh(model));
	}

	void writeMD5Anim(const Model &model, const std::string &path)
	{
		mt::io::saveFile(path, encodeMD5Anim(model));
	}
}
