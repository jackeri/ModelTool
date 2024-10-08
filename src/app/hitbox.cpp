#include "hitbox.h"
#include "script_stream.h"

namespace mt::model {

	Hitbox::Hitbox(float size) : Bounds(), Point()
	{
		if (size > 0)
		{
			float tmp = size / 2;
			min = glm::vec3(-tmp);
			max = glm::vec3(tmp);
		}
		else
		{
			min = glm::vec3(0);
			max = glm::vec3(0);
		}
	}

	void Hitbox::loadHitboxes(const Ref<io::MTFile> &file, std::vector<Hitbox> &list)
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
						hitbox.modifiers.emplace(stream.token());
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

				default: throw mt_ex("Unexpected token: " + token);
				}
			}

			if (stream.token() != "}")
			{
				throw mt_ex("Invalid hierarchy data");
			}

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

		out << "// Hitboxes exported with ModelTool" << nl;

		for (auto &box : list)
		{
			out << "hitbox {" << nl;

			out << "\tname \"" << box.name << "\"" << nl;

			out << "\torigin " << box.parentJoint << " ( " << box.location.x << " " << box.location.y << " " << box.location.z << " ) ( "
				<< box.rotation.x << " " << box.rotation.y << " " << box.rotation.z << " " << box.rotation.w << " )" << nl;

			out << "\tbounds ( " << box.min.x << " " << box.min.y << " " << box.min.z << " ) ( " << box.max.x << " " << box.max.y << " "
				<< box.max.z << " )" << nl;

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
