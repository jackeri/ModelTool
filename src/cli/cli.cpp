#include "cli.h"
#include "mt_version.h"
#include "filesystem.h"
#include "hitbox.h"
#include "lod.h"
#include "model.h"
#include "model_md3.h"
#include "morph_model.h"
#include "normals.h"
#include "player.h"
#include "skin.h"
#include "posed_model.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"

#include <filesystem>
#include <iomanip>
#include <map>


namespace mt::cli {

	namespace {

		/*! Usage error, reported with the usage text */
		struct UsageError : std::runtime_error {
			using std::runtime_error::runtime_error;
		};

		/*! Parsed command line */
		struct Options {
			std::string command{};
			std::vector<std::string> arguments{};  ///< arguments of the command (files)
			std::vector<std::string> basePaths{};  ///< --base
			std::vector<std::string> animations{}; ///< --animation
			std::string hitboxes{};				   ///< --hitboxes
			std::string saveHitboxes{};			   ///< --save-hitboxes
			bool hasFrames = false;				   ///< --frames was given
			int firstFrame = 0, lastFrame = -1;	   ///< --frames first:last
			float scale = 1.0f;					   ///< --scale
			std::string normals{};				   ///< --normals smooth|flat
			bool optimize = false;				   ///< --optimize
			bool caseSensitive = false;			   ///< --case-sensitive
			bool lod = false;					   ///< --lod
			bool help = false;					   ///< --help
			bool version = false;				   ///< --version
		};

		int parseInt(const std::string &text, const std::string &option)
		{
			try
			{
				size_t used = 0;
				int value = std::stoi(text, &used);
				if (used == text.size())
				{
					return value;
				}
			}
			catch (const std::exception &)
			{}
			throw UsageError("Invalid number for " + option + ": " + text);
		}

		float parseFloat(const std::string &text, const std::string &option)
		{
			try
			{
				size_t used = 0;
				float value = std::stof(text, &used);
				if (used == text.size())
				{
					return value;
				}
			}
			catch (const std::exception &)
			{}
			throw UsageError("Invalid number for " + option + ": " + text);
		}

		Options parse(const std::vector<std::string> &args)
		{
			Options options;
			for (size_t i = 0; i < args.size(); i++)
			{
				const std::string &arg = args[i];
				auto value = [&]() -> std::string {
					if (i + 1 >= args.size())
					{
						throw UsageError("Missing value for " + arg);
					}
					return args[++i];
				};

				if (arg == "--help" || arg == "-h")
				{
					options.help = true;
				}
				else if (arg == "--version")
				{
					options.version = true;
				}
				else if (arg == "--base")
				{
					options.basePaths.push_back(value());
				}
				else if (arg == "--animation")
				{
					options.animations.push_back(value());
				}
				else if (arg == "--hitboxes")
				{
					options.hitboxes = value();
				}
				else if (arg == "--save-hitboxes")
				{
					options.saveHitboxes = value();
				}
				else if (arg == "--frames")
				{
					std::string range = value();
					auto colon = range.find(':');
					if (colon == std::string::npos)
					{
						throw UsageError("Invalid frame range, expected first:last: " + range);
					}
					options.hasFrames = true;
					options.firstFrame = parseInt(range.substr(0, colon), arg);
					options.lastFrame = parseInt(range.substr(colon + 1), arg);
					if (options.firstFrame < 0 || options.lastFrame < options.firstFrame)
					{
						throw UsageError("Invalid frame range: " + range);
					}
				}
				else if (arg == "--scale")
				{
					options.scale = parseFloat(value(), arg);
					if (!(options.scale > 0))
					{
						throw UsageError("The scale must be larger than 0");
					}
				}
				else if (arg == "--normals")
				{
					options.normals = value();
					if (options.normals != "smooth" && options.normals != "flat")
					{
						throw UsageError("Invalid normals mode, expected smooth or flat: " + options.normals);
					}
				}
				else if (arg == "--case-sensitive")
				{
					options.caseSensitive = true;
				}
				else if (arg == "--optimize")
				{
					options.optimize = true;
				}
				else if (arg == "--lod")
				{
					options.lod = true;
				}
				else if (arg.size() > 1 && arg[0] == '-')
				{
					throw UsageError("Unknown option: " + arg);
				}
				else if (options.command.empty())
				{
					options.command = arg;
				}
				else
				{
					options.arguments.push_back(arg);
				}
			}
			return options;
		}

		/*! Files from the base folders and pk3 files, or from the disk */
		class Files {
		  public:
			explicit Files(const std::vector<std::string> &basePaths, bool caseSensitive = false)
			{
				system.setCaseInsensitive(!caseSensitive);
				for (const auto &path : basePaths)
				{
					if (!system.addPath(path))
					{
						throw mt_ex("Base folder or pk3 file not found: " + path);
					}
				}
			}

			/**
			 * Path to load a file with: the path in the base folders, or the absolute path of a file on the disk
			 */
			std::string resolve(const std::string &path)
			{
				if (system.hasSources() && system.findFile(path))
				{
					return path;
				}
				std::error_code error;
				if (std::filesystem::is_regular_file(path, error))
				{
					return std::filesystem::absolute(path, error).lexically_normal().string();
				}
				throw mt_ex("File not found: " + path);
			}

			Ref<mt::io::MTFile> load(const std::string &path)
			{
				auto file = system.loadFile(resolve(path));
				if (!file)
				{
					throw mt_ex("Can not read: " + path);
				}
				return file;
			}

			mt::io::FileSystem system{};
		};

		std::unique_ptr<Model> loadModel(Files &files, const Options &options, std::ostream &err)
		{
			if (options.arguments.empty())
			{
				throw UsageError("Missing the model file");
			}

			const std::string path = files.resolve(options.arguments[0]);
			std::unique_ptr<Model> loaded(model::io::loadModel(files.load(path)));

			if (auto *morph = dynamic_cast<model::morph::MorphModel *>(loaded.get()))
			{
				for (const auto &problem : model::loadMorphLods(*morph, files.system, path))
				{
					err << "Warning: " << problem << "\n";
				}
			}

			for (const auto &animation : options.animations)
			{
				model::io::loadAnimation(loaded.get(), files.load(animation));
			}

			if (!options.hitboxes.empty())
			{
				auto *hitboxes = loaded->getHitboxes();
				if (!hitboxes)
				{
					throw mt_ex("Only skeletal models have hitboxes");
				}
				hitboxes->clear();
				model::Hitbox::loadHitboxes(files.load(options.hitboxes), *hitboxes);
			}

			return loaded;
		}

		std::string join(const std::vector<std::string> &list)
		{
			std::string text;
			for (const auto &item : list)
			{
				text += (text.empty() ? "" : " ") + item;
			}
			return text.empty() ? "-" : text;
		}

		std::string vec(const glm::vec3 &value)
		{
			std::ostringstream text;
			text << std::fixed << std::setprecision(2) << "(" << value.x << " " << value.y << " " << value.z << ")";
			return text.str();
		}

		int formatsCommand(std::ostream &out)
		{
			out << "Models:     " << join(model::io::modelExtensions()) << "\n";
			out << "Animations: " << join(model::io::animationExtensions()) << "\n";
			out << "Write models:     " << join(model::io::modelWriterExtensions()) << "\n";
			out << "Write animations: " << join(model::io::animationWriterExtensions()) << "\n";
			return EXIT_OK;
		}

		void printBounds(const Model &model, std::ostream &out)
		{
			model::PoseOptions pose;
			pose.bindPose = model.skeletalModel() && !model.hasFrames();
			model::PosedModel posed = model::poseModel(model, pose);

			bool any = false;
			Bounds bounds{};
			for (const auto &mesh : posed.meshes)
			{
				for (const auto &position : mesh.positions)
				{
					bounds.min = any ? glm::min(bounds.min, position) : position;
					bounds.max = any ? glm::max(bounds.max, position) : position;
					any = true;
				}
			}
			if (any)
			{
				out << "Mesh bounds (" << (pose.bindPose ? "bind pose" : "frame 0") << "): " << vec(bounds.min) << " " << vec(bounds.max)
					<< "\n";
			}
		}

		template<typename Mesh>
		void printMeshes(const std::vector<Mesh> &meshes, std::ostream &out)
		{
			out << "Meshes: " << meshes.size() << "\n";
			for (size_t i = 0; i < meshes.size(); i++)
			{
				const auto &mesh = meshes[i];
				size_t vertices = 0;
				if constexpr (std::is_same_v<Mesh, model::morph::Mesh>)
				{
					vertices = mesh.texCoords.size();
				}
				else
				{
					vertices = mesh.vertices.size();
				}
				out << "  " << i << ": " << mesh.name << "  vertices " << vertices << ", triangles " << mesh.tris.size() << ", material "
					<< (mesh.materialName.empty() ? "-" : mesh.materialName) << "\n";
			}
		}

		int playerInfo(Files &files, const std::string &path, std::ostream &out)
		{
			const std::string resolved = files.resolve(path);
			model::PlayerFiles player;
			if (!model::findPlayerFiles(files.system, resolved, player))
			{
				throw mt_ex("No player model (lower.md3, upper.md3 and head.md3) next to " + path);
			}
			auto config = model::parsePlayerConfig(model::fileText(files.load(resolved)));

			out << "Player: " << path << "\n";
			for (const auto &[part, file] : {std::pair{"Lower", player.lower}, {"Upper", player.upper}, {"Head", player.head}})
			{
				std::unique_ptr<Model> partModel(model::io::loadModel(files.load(file)));
				out << part << ": " << file << "  frames " << partModel->numFrames() << "\n";
			}
			out << "Skins: " << join(player.skins) << "\n";
			out << "Sex: " << config.sex << ", footsteps: " << config.footsteps << "\n";
			out << "Sequences: " << config.animations.size() << "\n";
			for (const auto &animation : config.animations)
			{
				const char *part = animation.part == model::PlayerPart::Both   ? "both"
								   : animation.part == model::PlayerPart::Legs ? "legs"
																			   : "torso";
				out << "  " << animation.name << "  " << part << " frames " << animation.firstFrame << "-"
					<< animation.firstFrame + animation.numFrames - 1 << ", " << animation.fps << " fps"
					<< (animation.loopFrames > 0 ? ", loops" : "") << (animation.reversed ? ", reversed" : "") << "\n";
			}
			return EXIT_OK;
		}

		int infoCommand(const Options &options, std::ostream &out, std::ostream &err)
		{
			Files files(options.basePaths, options.caseSensitive);
			if (!options.arguments.empty() && model::io::normalizeExtension(options.arguments[0]) == ".cfg")
			{
				return playerInfo(files, options.arguments[0], out);
			}
			auto loaded = loadModel(files, options, err);
			const Model &model = *loaded;

			out << "File: " << options.arguments[0] << "\n";
			if (!model.name.empty())
			{
				out << "Name: " << model.name << "\n";
			}
			out << "Type: " << (model.skeletalModel() ? "skeletal (bone animated)" : "morph (vertex animated)") << "\n";
			out << "Frames: " << model.numFrames() << "\n";

			if (const auto *morph = dynamic_cast<const model::morph::MorphModel *>(&model))
			{
				printMeshes(morph->meshes, out);
				out << "Levels of detail: " << morph->lodCount() << "\n";
				out << "Tags: " << morph->tags.size() << "\n";
				for (const auto &tag : morph->tags)
				{
					out << "  " << tag.name << "\n";
				}
			}
			else if (const auto *skeletal = dynamic_cast<const model::skeletal::SkeletalModel *>(&model))
			{
				printMeshes(skeletal->meshes, out);
				out << "Bones: " << skeletal->joints.size() << "\n";
				for (size_t i = 0; i < skeletal->joints.size(); i++)
				{
					const auto &joint = skeletal->joints[i];
					const auto parent = joint.parentId >= 0 && joint.parentId < int(skeletal->joints.size())
											? skeletal->joints[joint.parentId].name
											: std::string("-");
					out << "  " << i << ": " << joint.name << "  parent " << parent << (joint.asTag ? ", tag" : "") << "\n";
				}
				out << "Tags: " << skeletal->tags.size() << "\n";
				for (const auto &tag : skeletal->tags)
				{
					const auto bone = tag.parentBone >= 0 && tag.parentBone < int(skeletal->joints.size())
										  ? skeletal->joints[tag.parentBone].name
										  : std::string("-");
					out << "  " << tag.name << "  bone " << bone << "\n";
				}
				out << "Hitboxes: " << skeletal->hitboxes.size() << "\n";
				for (const auto &hitbox : skeletal->hitboxes)
				{
					out << "  " << (hitbox.name.empty() ? "(no name)" : hitbox.name) << "  " << model::hitboxShapeName(hitbox.shape)
						<< "\n";
				}
			}

			out << "Animation sequences: " << model.animations.size() << "\n";
			for (const auto &animation : model.animations)
			{
				out << "  " << animation.name << "  frames " << animation.startFrame << "-" << animation.lastFrame() << ", "
					<< animation.fps << " fps" << (animation.loop ? ", loops" : "") << "\n";
			}

			printBounds(model, out);
			return EXIT_OK;
		}

		int convertCommand(const Options &options, std::ostream &out, std::ostream &err)
		{
			if (options.arguments.size() < 2 && options.saveHitboxes.empty())
			{
				throw UsageError("Missing the output file");
			}

			Files files(options.basePaths, options.caseSensitive);
			auto loaded = loadModel(files, options, err);
			Model &model = *loaded;

			if (options.hasFrames)
			{
				if (options.lastFrame >= model.numFrames())
				{
					throw mt_ex("The model has " + std::to_string(model.numFrames()) + " frames");
				}
				model.firstExportFrame = options.firstFrame;
				model.lastExportFrame = options.lastFrame;
			}

			model.scale *= options.scale;

			if (!options.normals.empty())
			{
				auto mode = options.normals == "flat" ? model::NormalMode::Flat : model::NormalMode::Smooth;
				if (!model::calculateNormals(model, mode))
				{
					throw mt_ex("Normals can not be calculated for this model");
				}
			}

			if (options.optimize)
			{
				auto *skeletal = dynamic_cast<model::skeletal::SkeletalModel *>(&model);
				if (!skeletal)
				{
					throw mt_ex("--optimize needs a skeletal model");
				}
				out << "Removed " << model::skeletal::optimizeVertices(*skeletal) << " duplicate vertices\n";
			}

			if (options.lod && !model::calculateLod(model))
			{
				throw mt_ex("The levels of detail can not be calculated for this model");
			}

			for (size_t i = 1; i < options.arguments.size(); i++)
			{
				const std::string &output = options.arguments[i];
				// Animation only formats (like .md5anim or .aninc) use the animation writers
				const std::string ext = model::io::normalizeExtension(output);
				const auto modelWriters = model::io::modelWriterExtensions();
				const auto animationWriters = model::io::animationWriterExtensions();
				if (std::find(modelWriters.begin(), modelWriters.end(), ext) != modelWriters.end())
				{
					model::io::writeModel(model, output);
				}
				else if (std::find(animationWriters.begin(), animationWriters.end(), ext) != animationWriters.end())
				{
					model::io::writeAnimation(model, output);
				}
				else
				{
					throw mt_ex("No writer for " + (ext.empty() ? output : ext) + ", see mt formats");
				}
				out << "Wrote " << output << "\n";
			}

			if (!options.saveHitboxes.empty())
			{
				auto *hitboxes = model.getHitboxes();
				if (!hitboxes)
				{
					throw mt_ex("Only skeletal models have hitboxes");
				}
				model::Hitbox::saveHitboxes(options.saveHitboxes, *hitboxes);
				out << "Wrote " << options.saveHitboxes << "\n";
			}
			return EXIT_OK;
		}

		void
		listFolder(mt::io::FileSystem &system, const std::string &folder, const std::vector<std::string> &extensions, std::ostream &out)
		{
			auto records = system.getFiles(folder);
			for (const auto &record : *records)
			{
				const std::string path = folder.empty() ? record.name : folder + "/" + record.name;
				if (record.isDirectory)
				{
					listFolder(system, path, extensions, out);
				}
				else if (std::find(extensions.begin(), extensions.end(), model::io::normalizeExtension(record.name)) != extensions.end())
				{
					out << path << "\n";
				}
			}
		}

		int listCommand(const Options &options, std::ostream &out)
		{
			if (options.basePaths.empty())
			{
				throw UsageError("list needs at least one --base folder or pk3 file");
			}
			Files files(options.basePaths, options.caseSensitive);

			std::string folder = options.arguments.empty() ? "" : options.arguments[0];
			while (!folder.empty() && folder.back() == '/')
			{
				folder.pop_back();
			}

			listFolder(files.system, folder, model::io::modelExtensions(), out);
			return EXIT_OK;
		}
	}

	std::string usage()
	{
		return "Usage: mt [--base folder]... <command> [options]\n"
			   "\n"
			   "Commands:\n"
			   "  formats                        list the readable and writable formats\n"
			   "  info <model>                   show the meshes, bones, tags, hitboxes and animation sequences;\n"
			   "                                 animation.cfg shows a player model (parts, skins, sequences)\n"
			   "  convert <model> <output>...    write the model, the format comes from the output extension\n"
			   "  list [folder]                  list the model files in the base folders and pk3 files\n"
			   "\n"
			   "Options:\n"
			   "  --base folder         base (mod) folder or pk3 file to load files from, repeatable;\n"
			   "                        files not found there are read from the disk\n"
			   "  --animation file      load an animation into the model (md5anim, mdx, aninc, ...), repeatable\n"
			   "  --hitboxes file       load hitboxes into the model (.hitboxes or .htx)\n"
			   "  --frames first:last   convert: frames to write\n"
			   "  --scale s             convert: scale the model\n"
			   "  --normals smooth|flat convert: recalculate the normals\n"
			   "  --optimize            convert: merge duplicate vertices (skeletal models)\n"
			   "  --lod                 convert: calculate the level of detail collapse maps\n"
			   "  --save-hitboxes file  convert: write the hitboxes\n"
			   "  --case-sensitive      only find files with the exact case of their names (the game ignores the case)\n"
			   "  --help, --version\n";
	}

	int runCli(const std::vector<std::string> &args, std::ostream &out, std::ostream &err)
	{
		try
		{
			Options options = parse(args);

			if (options.version)
			{
				out << "mt " << MT_VERSION << "\n";
				return EXIT_OK;
			}
			if (options.help || options.command.empty())
			{
				(options.help ? out : err) << usage();
				return options.help ? EXIT_OK : EXIT_USAGE;
			}

			if (options.command == "formats")
			{
				return formatsCommand(out);
			}
			if (options.command == "info")
			{
				return infoCommand(options, out, err);
			}
			if (options.command == "convert")
			{
				return convertCommand(options, out, err);
			}
			if (options.command == "list")
			{
				return listCommand(options, out);
			}
			throw UsageError("Unknown command: " + options.command);
		}
		catch (const UsageError &ex)
		{
			err << "Error: " << ex.what() << "\n\n" << usage();
			return EXIT_USAGE;
		}
		catch (const std::exception &ex)
		{
			err << "Error: " << ex.what() << "\n";
			return EXIT_FAILED;
		}
	}
}
