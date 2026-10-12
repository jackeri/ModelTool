#include "model_stl.h"
#include "model_export.h"
#include "posed_model.h"
#include "binary_writer.h"

#include <iomanip>

namespace mt::model {

	std::vector<byte> encodeSTL(const Model &source, bool binary)
	{
		auto model = cloneModel(source);
		bakeTransform(*model);

		PoseOptions options;
		options.bindPose = model->skeletalModel() && (model->currentFrame == FRAME_BIND_POSE || !model->hasFrames());
		options.frame = options.nextFrame = std::max(model->currentFrame, 0);
		PosedModel posed = poseModel(*model, options);

		struct Facet {
			glm::vec3 normal;
			glm::vec3 vertices[3];
		};
		std::vector<Facet> facets;
		for (const auto &mesh : posed.meshes)
		{
			for (const auto &tri : mesh.tris)
			{
				// idTech triangles are clockwise, STL counter-clockwise
				const glm::vec3 &a = mesh.positions.at(size_t(tri.x));
				const glm::vec3 &b = mesh.positions.at(size_t(tri.z));
				const glm::vec3 &c = mesh.positions.at(size_t(tri.y));
				glm::vec3 normal = glm::cross(b - a, c - a);
				float length = glm::length(normal);
				facets.push_back({length > 0 ? normal / length : glm::vec3{0}, {a, b, c}});
			}
		}

		if (binary)
		{
			mt::io::BinaryWriter out;
			char header[80] = "Exported with ModelTool";
			out.write(header);
			out.write(uint32_t(facets.size()));
			for (const auto &facet : facets)
			{
				out.write(facet.normal);
				for (const auto &vertex : facet.vertices)
				{
					out.write(vertex);
				}
				out.write(uint16_t(0));
			}
			return out.data;
		}

		std::string name = model->name.empty() ? "model" : model->name;
		std::replace(name.begin(), name.end(), ' ', '_');

		std::ostringstream out;
		out << std::setprecision(9) << "solid " << name << "\n";
		for (const auto &facet : facets)
		{
			out << "facet normal " << facet.normal.x << " " << facet.normal.y << " " << facet.normal.z << "\n\touter loop\n";
			for (const auto &vertex : facet.vertices)
			{
				out << "\t\tvertex " << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
			}
			out << "\tendloop\nendfacet\n";
		}
		out << "endsolid " << name << "\n";

		std::string text = out.str();
		return {text.begin(), text.end()};
	}

	void writeSTL(const Model &model, const std::string &path, bool binary)
	{
		mt::io::saveFile(path, encodeSTL(model, binary));
	}
}
