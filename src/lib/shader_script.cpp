#include "shader_script.h"
#include "filesystem.h"
#include "skin.h"
#include "texture_lookup.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace mt::model {

	namespace {
		/*! Token of a shader script with the line it is on (keyword arguments end at the line break) */
		struct Token {
			std::string text;
			int line;
		};

		/*! Split a script into tokens: comments are removed, braces are tokens of their own, quotes group words */
		std::vector<Token> tokenize(const std::string &text)
		{
			std::vector<Token> tokens;
			int line = 0;
			size_t i = 0;
			const size_t size = text.size();
			while (i < size)
			{
				const char c = text[i];
				if (c == '\n')
				{
					line++;
					i++;
				}
				else if (std::isspace(static_cast<unsigned char>(c)))
				{
					i++;
				}
				else if (c == '/' && i + 1 < size && text[i + 1] == '/')
				{
					while (i < size && text[i] != '\n')
					{
						i++;
					}
				}
				else if (c == '/' && i + 1 < size && text[i + 1] == '*')
				{
					i += 2;
					while (i < size && !(text[i] == '*' && i + 1 < size && text[i + 1] == '/'))
					{
						line += text[i] == '\n' ? 1 : 0;
						i++;
					}
					i = std::min(i + 2, size);
				}
				else if (c == '{' || c == '}')
				{
					tokens.push_back({std::string(1, c), line});
					i++;
				}
				else if (c == '"')
				{
					const size_t end = text.find('"', i + 1);
					const size_t stop = end == std::string::npos ? size : end;
					tokens.push_back({text.substr(i + 1, stop - i - 1), line});
					i = std::min(stop + 1, size);
				}
				else
				{
					const size_t start = i;
					while (i < size && !std::isspace(static_cast<unsigned char>(text[i])) && text[i] != '{' && text[i] != '}'
						   && !(text[i] == '/' && i + 1 < size && (text[i + 1] == '/' || text[i + 1] == '*')))
					{
						i++;
					}
					tokens.push_back({text.substr(start, i - start), line});
				}
			}
			return tokens;
		}

		bool parseBlendFactor(const std::string &name, BlendFactor &out)
		{
			static const std::array<std::pair<const char *, BlendFactor>, 11> factors = {{
				{"gl_zero", BlendFactor::Zero},
				{"gl_one", BlendFactor::One},
				{"gl_src_color", BlendFactor::SrcColor},
				{"gl_one_minus_src_color", BlendFactor::OneMinusSrcColor},
				{"gl_dst_color", BlendFactor::DstColor},
				{"gl_one_minus_dst_color", BlendFactor::OneMinusDstColor},
				{"gl_src_alpha", BlendFactor::SrcAlpha},
				{"gl_one_minus_src_alpha", BlendFactor::OneMinusSrcAlpha},
				{"gl_dst_alpha", BlendFactor::DstAlpha},
				{"gl_one_minus_dst_alpha", BlendFactor::OneMinusDstAlpha},
				{"gl_src_alpha_saturate", BlendFactor::SrcAlphaSaturate},
			}};
			const std::string lower = tools::lowercase(name);
			for (const auto &[text, factor] : factors)
			{
				if (lower == text)
				{
					out = factor;
					return true;
				}
			}
			return false;
		}

		/*! What a stage draws, the first stage with an image is used for the shader */
		struct Stage {
			std::string image;
			std::vector<std::string> animFrames;
			float animFrequency = 0.0f;
			bool clamp = false;
			bool environment = false;
			AlphaFunc alphaFunc = AlphaFunc::None;
			bool blend = false;
			BlendFactor blendSrc = BlendFactor::One;
			BlendFactor blendDst = BlendFactor::Zero;
			bool depthWrite = false;
		};

		/*! Images that are not files: lightmaps and the renderer's built-in images */
		bool isImage(const std::string &name)
		{
			return !name.empty() && name[0] != '$' && name[0] != '*';
		}

		class Parser {
		  public:
			Parser(const std::string &text, std::string file) : tokens(tokenize(text)), file(std::move(file)) {}

			std::vector<ShaderDef> parse()
			{
				std::vector<ShaderDef> shaders;
				while (pos < tokens.size())
				{
					ShaderDef shader;
					shader.name = tokens[pos++].text;
					shader.file = file;
					if (shader.name == "{" || shader.name == "}" || pos >= tokens.size() || tokens[pos].text != "{")
					{
						// Broken file: the game stops reading it here
						break;
					}
					pos++;
					if (!parseBody(shader))
					{
						break;
					}
					shaders.push_back(std::move(shader));
				}
				return shaders;
			}

		  private:
			std::vector<Token> tokens;
			std::string file;
			size_t pos = 0;

			/*! Arguments of the keyword at pos - 1: the tokens after it on the same line, braces end them */
			std::vector<std::string> arguments()
			{
				std::vector<std::string> out;
				const int line = tokens[pos - 1].line;
				while (pos < tokens.size() && tokens[pos].line == line && tokens[pos].text != "{" && tokens[pos].text != "}")
				{
					out.push_back(tokens[pos++].text);
				}
				return out;
			}

			bool parseBody(ShaderDef &shader)
			{
				bool haveImage = false;
				while (pos < tokens.size())
				{
					const std::string token = tokens[pos++].text;
					if (token == "}")
					{
						return true;
					}
					if (token == "{")
					{
						Stage stage;
						if (!parseStage(stage))
						{
							return false;
						}
						shader.stages++;
						if (!haveImage && isImage(stage.image))
						{
							haveImage = true;
							// After a lightmap or white base stage the image stage blends onto it (like "filter"): on its
							// own it is drawn opaque
							if (shader.stages > 1)
							{
								stage.blend = false;
								stage.depthWrite = true;
							}
							useStage(shader, stage);
						}
						continue;
					}

					const std::string keyword = tools::lowercase(token);
					const auto args = arguments();
					const std::string first = args.empty() ? "" : args[0];
					if (keyword == "cull")
					{
						const std::string side = tools::lowercase(first);
						if (side == "none" || side == "twosided" || side == "disable")
						{
							shader.cull = ShaderCull::None;
						}
						else if (side == "back" || side == "backside" || side == "backsided")
						{
							shader.cull = ShaderCull::Back;
						}
						else
						{
							shader.cull = ShaderCull::Front;
						}
					}
					else if (keyword == "qer_editorimage")
					{
						shader.editorImage = first;
					}
					else if (keyword == "surfaceparm" && !first.empty())
					{
						shader.surfaceParms.push_back(tools::lowercase(first));
					}
					else if (keyword == "skyparms")
					{
						shader.skyBox = first == "-" ? std::string() : first;
					}
					else if (keyword == "nopicmip")
					{
						shader.noPicmip = true;
					}
					else if (keyword == "implicitmap" || keyword == "implicitmask" || keyword == "implicitblend")
					{
						// One stage with the image ("-" is the shader name): opaque, alpha tested or blended
						Stage stage;
						stage.image = first == "-" ? shader.name : first;
						stage.depthWrite = true;
						if (keyword == "implicitmask")
						{
							stage.alphaFunc = AlphaFunc::GE128;
						}
						else if (keyword == "implicitblend")
						{
							stage.blend = true;
							stage.blendSrc = BlendFactor::SrcAlpha;
							stage.blendDst = BlendFactor::OneMinusSrcAlpha;
							stage.depthWrite = false;
						}
						shader.stages++;
						if (!haveImage && isImage(stage.image))
						{
							haveImage = true;
							useStage(shader, stage);
						}
					}
				}
				return false;
			}

			bool parseStage(Stage &stage)
			{
				bool depthWriteSet = false;
				while (pos < tokens.size())
				{
					const std::string token = tokens[pos++].text;
					if (token == "}")
					{
						// Blended stages do not write the depth unless they ask for it
						if (!depthWriteSet)
						{
							stage.depthWrite = !stage.blend;
						}
						return true;
					}
					if (token == "{")
					{
						return false;
					}

					const std::string keyword = tools::lowercase(token);
					const auto args = arguments();
					const std::string first = args.empty() ? "" : args[0];
					if (keyword == "map" || keyword == "clampmap")
					{
						stage.image = first;
						stage.clamp = keyword == "clampmap";
					}
					else if (keyword == "animmap" && args.size() >= 2)
					{
						stage.animFrequency = std::strtof(args[0].c_str(), nullptr);
						stage.animFrames.assign(args.begin() + 1, args.end());
						stage.image = stage.animFrames.front();
					}
					else if (keyword == "alphafunc")
					{
						const std::string func = tools::lowercase(first);
						stage.alphaFunc = func == "gt0"		? AlphaFunc::GT0
										  : func == "lt128" ? AlphaFunc::LT128
										  : func == "ge128" ? AlphaFunc::GE128
															: AlphaFunc::None;
					}
					else if (keyword == "blendfunc" && !args.empty())
					{
						const std::string mode = tools::lowercase(first);
						stage.blend = true;
						if (mode == "add")
						{
							stage.blendSrc = BlendFactor::One;
							stage.blendDst = BlendFactor::One;
						}
						else if (mode == "filter")
						{
							stage.blendSrc = BlendFactor::DstColor;
							stage.blendDst = BlendFactor::Zero;
						}
						else if (mode == "blend")
						{
							stage.blendSrc = BlendFactor::SrcAlpha;
							stage.blendDst = BlendFactor::OneMinusSrcAlpha;
						}
						else if (args.size() < 2 || !parseBlendFactor(args[0], stage.blendSrc)
								 || !parseBlendFactor(args[1], stage.blendDst))
						{
							stage.blend = false;
						}
						// GL_ONE GL_ZERO is the same as no blending
						if (stage.blend && stage.blendSrc == BlendFactor::One && stage.blendDst == BlendFactor::Zero)
						{
							stage.blend = false;
						}
					}
					else if (keyword == "tcgen" || keyword == "texgen")
					{
						stage.environment = tools::lowercase(first) == "environment";
					}
					else if (keyword == "depthwrite")
					{
						stage.depthWrite = true;
						depthWriteSet = true;
					}
				}
				return false;
			}

			static void useStage(ShaderDef &shader, const Stage &stage)
			{
				shader.texture = stage.image;
				shader.animFrames = stage.animFrames;
				shader.animFrequency = stage.animFrequency;
				shader.clamp = stage.clamp;
				shader.environment = stage.environment;
				shader.alphaFunc = stage.alphaFunc;
				shader.blend = stage.blend;
				shader.blendSrc = stage.blendSrc;
				shader.blendDst = stage.blendDst;
				shader.depthWrite = stage.depthWrite;
			}
		};
	}

	std::string shaderKey(const std::string &name)
	{
		std::string key = tools::lowercase(name);
		std::replace(key.begin(), key.end(), '\\', '/');
		// COM_StripExtension: only an extension in the last path part
		const size_t dot = key.find_last_of('.');
		const size_t slash = key.find_last_of('/');
		if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
		{
			key.erase(dot);
		}
		return key;
	}

	std::vector<ShaderDef> parseShaderScript(const std::string &text, const std::string &file)
	{
		return Parser(text, file).parse();
	}

	size_t ShaderLibrary::load(mt::io::FileSystem &files)
	{
		clear();
		size_t count = 0;
		for (const auto &entry : files.listFolder("scripts"))
		{
			const std::string lower = tools::lowercase(entry.name);
			if (entry.isDirectory || lower.size() < 7 || lower.compare(lower.size() - 7, 7, ".shader") != 0)
			{
				continue;
			}
			const std::string path = "scripts/" + entry.name;
			auto file = files.loadFile(path);
			if (!file || !file->valid())
			{
				continue;
			}
			// listFolder sorts by name, so later files replace the shaders of earlier ones
			add(parseShaderScript(fileText(file), path));
			count++;
		}
		return count;
	}

	void ShaderLibrary::add(const std::vector<ShaderDef> &list)
	{
		// Inside one script the first shader with a name counts
		std::unordered_map<std::string, bool> seen;
		for (const auto &shader : list)
		{
			const std::string key = shaderKey(shader.name);
			if (seen.count(key))
			{
				continue;
			}
			seen[key] = true;
			shaders[key] = shader;
		}
	}

	const ShaderDef *ShaderLibrary::find(const std::string &material) const
	{
		auto found = shaders.find(shaderKey(material));
		return found == shaders.end() ? nullptr : &found->second;
	}

	std::string
	findMaterialTexture(mt::io::FileSource &source, const ShaderLibrary &shaders, const std::string &material, const std::string &modelPath)
	{
		if (const ShaderDef *shader = shaders.find(material))
		{
			const std::string image = shader->image();
			if (isImage(image))
			{
				std::string path = mt::io::findTexture(source, image, "");
				if (!path.empty())
				{
					return path;
				}
			}
		}
		return mt::io::findTexture(source, material, modelPath);
	}

	std::vector<std::string> ShaderLibrary::skyBoxes() const
	{
		std::vector<std::string> boxes;
		for (const auto &[key, shader] : shaders)
		{
			if (!shader.skyBox.empty() && std::find(boxes.begin(), boxes.end(), shader.skyBox) == boxes.end())
			{
				boxes.push_back(shader.skyBox);
			}
		}
		std::sort(boxes.begin(), boxes.end());
		return boxes;
	}
}
