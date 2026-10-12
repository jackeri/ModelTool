#include "hitbox.h"
#include "script_stream.h"

#include <iomanip>

namespace mt::model {

	const char *hitboxShapeName(HitboxShape shape)
	{
		switch (shape)
		{
		case HitboxShape::Sphere: return "sphere";
		case HitboxShape::Cylinder: return "cylinder";
		case HitboxShape::Capsule: return "capsule";
		case HitboxShape::Box:
		default: return "box";
		}
	}

	bool parseHitboxShape(const std::string &name, HitboxShape &shape)
	{
		std::string lower = name;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		for (auto candidate : {HitboxShape::Box, HitboxShape::Sphere, HitboxShape::Cylinder, HitboxShape::Capsule})
		{
			if (lower == hitboxShapeName(candidate))
			{
				shape = candidate;
				return true;
			}
		}
		return false;
	}

	Hitbox::Hitbox(float size) : Bounds(), Point()
	{
		size = std::max(size, 0.0f);
		min = glm::vec3(-size / 2);
		max = glm::vec3(size / 2);
		radius = size / 2;
		height = size;
	}

	void Hitbox::setShape(HitboxShape newShape)
	{
		if (newShape == shape)
		{
			return;
		}

		if (shape == HitboxShape::Box)
		{
			// The round shapes take the box's width and height
			glm::vec3 size = max - min;
			radius = std::max(size.x, size.y) / 2;
			height = size.z;
			if (newShape == HitboxShape::Sphere)
			{
				radius = std::max(radius, size.z / 2);
			}
		}
		else if (newShape == HitboxShape::Box)
		{
			Bounds bounds = localBounds();
			min = bounds.min;
			max = bounds.max;
		}
		else if (shape == HitboxShape::Sphere)
		{
			height = radius * 2;
		}

		shape = newShape;
		clampSize();
	}

	void Hitbox::clampSize()
	{
		radius = std::max(radius, 0.0f);
		height = std::max(height, 0.0f);
		if (shape == HitboxShape::Capsule)
		{
			height = std::max(height, radius * 2);
		}
		const glm::vec3 low = glm::min(min, max);
		max = glm::max(min, max);
		min = low;
	}

	Bounds Hitbox::localBounds() const
	{
		switch (shape)
		{
		case HitboxShape::Sphere: return {glm::vec3(-radius), glm::vec3(radius)};
		case HitboxShape::Cylinder:
		case HitboxShape::Capsule: return {{-radius, -radius, -height / 2}, {radius, radius, height / 2}};
		case HitboxShape::Box:
		default: return {min, max};
		}
	}

	std::vector<std::pair<glm::vec3, glm::vec3>> Hitbox::lines(int segments) const
	{
		segments = std::max(segments, 4);
		std::vector<std::pair<glm::vec3, glm::vec3>> out;
		const float step = glm::two_pi<float>() / float(segments);

		// Circle (or arc) around a center, a and b are the unit axes of the circle's plane
		auto arc = [&](const glm::vec3 &center, const glm::vec3 &a, const glm::vec3 &b, float r, float from, float to) {
			const int count = std::max(1, int(std::ceil((to - from) / step)));
			const float delta = (to - from) / float(count);
			for (int i = 0; i < count; i++)
			{
				float t0 = from + delta * float(i), t1 = from + delta * float(i + 1);
				out.emplace_back(center + (a * std::cos(t0) + b * std::sin(t0)) * r, center + (a * std::cos(t1) + b * std::sin(t1)) * r);
			}
		};

		const glm::vec3 x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
		const float full = glm::two_pi<float>();
		const float pi = glm::pi<float>();

		switch (shape)
		{
		case HitboxShape::Box: {
			const glm::vec3 &a = min, &b = max;
			const glm::vec3 corners[8] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z},
										  {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}};
			const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
			for (const auto &edge : edges)
			{
				out.emplace_back(corners[edge[0]], corners[edge[1]]);
			}
			break;
		}
		case HitboxShape::Sphere: {
			arc({}, x, y, radius, 0, full);
			arc({}, x, z, radius, 0, full);
			arc({}, y, z, radius, 0, full);
			// Two latitude rings make the round shape easier to read
			for (float latitude : {-pi / 4, pi / 4})
			{
				arc(z * radius * std::sin(latitude), x, y, radius * std::cos(latitude), 0, full);
			}
			break;
		}
		case HitboxShape::Cylinder: {
			const float half = height / 2;
			arc(z * half, x, y, radius, 0, full);
			arc(-z * half, x, y, radius, 0, full);
			for (int i = 0; i < 8; i++)
			{
				float angle = full * float(i) / 8.0f;
				glm::vec3 side = (x * std::cos(angle) + y * std::sin(angle)) * radius;
				out.emplace_back(side - z * half, side + z * half);
			}
			break;
		}
		case HitboxShape::Capsule: {
			// The height includes the end caps, the straight part is between the cap centres
			const float half = std::max(height / 2 - radius, 0.0f);
			arc(z * half, x, y, radius, 0, full);
			arc(-z * half, x, y, radius, 0, full);
			for (int i = 0; i < 4; i++)
			{
				float angle = full * float(i) / 4.0f;
				glm::vec3 side = (x * std::cos(angle) + y * std::sin(angle)) * radius;
				out.emplace_back(side - z * half, side + z * half);
			}
			for (const auto &axis : {x, y})
			{
				arc(z * half, axis, z, radius, 0, pi);
				arc(-z * half, axis, -z, radius, 0, pi);
			}
			break;
		}
		}

		return out;
	}

	void Hitbox::loadHitboxes(const Ref<mt::io::MTFile> &file, std::vector<Hitbox> &list)
	{
		ScriptStream stream(file);

		while (stream.hasNext())
		{
			if (stream.token() != "hitbox")
			{
				throw mt_ex("Invalid file data, expected 'hitboxes'");
			}

			if (stream.token() != "{")
			{
				throw mt_ex("Invalid file data, expected '{'");
			}

			Hitbox hitbox;
			while (stream.hasNext() && stream.peekNext() != "}")
			{
				std::string token = stream.token();

				switch (hash(token.c_str()))
				{
				case hash("name"): hitbox.name = stream.token(); break;

				case hash("modifiers"): {
					if (stream.token() != "{")
					{
						throw mt_ex("Invalid hierarchy data");
					}

					while (stream.hasNext() && stream.peekNext() != "}")
					{
						std::string modifier = stream.token();

						// The old LegacyModelView format (.htx) has "name true|false" pairs, like "headShot true"
						std::string value = stream.peekNext();
						std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
						if (value == "true" || value == "false")
						{
							stream.token();
							if (value == "false")
							{
								continue;
							}
							std::transform(modifier.begin(), modifier.end(), modifier.begin(),
										   [](unsigned char c) { return char(std::tolower(c)); });
						}

						hitbox.modifiers.emplace(modifier);
					}

					if (stream.token() != "}")
					{
						throw mt_ex("Invalid hierarchy data");
					}

					break;
				}

				case hash("origin"): {
					hitbox.parentJoint = stream.parseInt();
					stream.parse1DMatrix(3, glm::value_ptr(hitbox.location));
					stream.parse1DMatrix(4, glm::value_ptr(hitbox.rotation));
					break;
				}

				case hash("bounds"): {
					stream.parse1DMatrix(3, glm::value_ptr(hitbox.min));
					stream.parse1DMatrix(3, glm::value_ptr(hitbox.max));
					break;
				}

				case hash("shape"): {
					if (!parseHitboxShape(stream.token(false), hitbox.shape))
					{
						throw mt_ex("Unknown hitbox shape: " + stream.last());
					}
					break;
				}

				case hash("radius"): hitbox.radius = stream.parseFloat(false); break;

				case hash("height"): hitbox.height = stream.parseFloat(false); break;

				// The old LegacyModelView format (.htx) stores the same data with different keywords
				case hash("location"): {
					stream.parse1DMatrix(3, glm::value_ptr(hitbox.location));
					stream.parse1DMatrix(4, glm::value_ptr(hitbox.rotation));
					break;
				}

				case hash("min"): stream.parse1DMatrix(3, glm::value_ptr(hitbox.min)); break;

				case hash("max"): stream.parse1DMatrix(3, glm::value_ptr(hitbox.max)); break;

				case hash("joint"): hitbox.parentJoint = stream.parseInt(); break;

				default: throw mt_ex("Unexpected token: " + token);
				}
			}

			if (stream.token() != "}")
			{
				throw mt_ex("Invalid hierarchy data");
			}

			hitbox.clampSize();
			list.emplace_back(hitbox);
		}
	}

	const char nl = '\n';

	void Hitbox::saveHitboxes(const std::string &path, std::vector<Hitbox> &list)
	{
		std::ofstream out;
		out.open(path);

		if (!out.is_open())
		{
			throw mt_ex("Invalid path: " + path);
		}

		out << std::setprecision(9);
		out << "// Hitboxes exported with ModelTool" << nl;

		for (auto &box : list)
		{
			out << "hitbox {" << nl;

			out << "\tname \"" << box.name << "\"" << nl;

			out << "\torigin " << box.parentJoint << " ( " << box.location.x << " " << box.location.y << " " << box.location.z << " ) ( "
				<< box.rotation.x << " " << box.rotation.y << " " << box.rotation.z << " " << box.rotation.w << " )" << nl;

			// Boxes are written as before, the other shapes add their shape and size
			if (box.shape == HitboxShape::Box)
			{
				out << "\tbounds ( " << box.min.x << " " << box.min.y << " " << box.min.z << " ) ( " << box.max.x << " " << box.max.y << " "
					<< box.max.z << " )" << nl;
			}
			else
			{
				out << "\tshape " << hitboxShapeName(box.shape) << nl;
				out << "\tradius " << box.radius << nl;
				if (box.shape != HitboxShape::Sphere)
				{
					out << "\theight " << box.height << nl;
				}
			}

			out << "\tmodifiers {" << nl;
			for (auto &mod : box.modifiers)
			{
				out << "\t\t" << mod << nl;
			}
			out << "\t}" << nl;

			out << "}" << nl;
		}

		out.close();
	}
}
