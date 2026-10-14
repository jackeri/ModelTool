#pragma once

#include "library.h"
#include "files.h"

#include <unordered_map>

namespace mt::io {
	class FileSystem;
}

namespace mt::model {

	/*! Which sides of the triangles a shader draws (the shader's "cull" keyword) */
	enum class ShaderCull
	{
		Front, ///< the front side only, the default
		Back,  ///< the back side only ("cull back")
		None   ///< both sides ("cull none", "cull twosided", "cull disable")
	};

	/*! Alpha test of a stage ("alphaFunc") */
	enum class AlphaFunc
	{
		None,
		GT0,
		LT128,
		GE128
	};

	/*! Blend factor of a stage ("blendFunc"), named like the GL constants */
	enum class BlendFactor
	{
		Zero,
		One,
		SrcColor,
		OneMinusSrcColor,
		DstColor,
		OneMinusDstColor,
		SrcAlpha,
		OneMinusSrcAlpha,
		DstAlpha,
		OneMinusDstAlpha,
		SrcAlphaSaturate
	};

	/**
	 * A Quake 3 / ET shader from a shader script (the scripts/<name>.shader files), with what a viewer needs: the image of the first
	 * stage that has one and how that stage is drawn. Lightmap and white image stages are skipped.
	 */
	struct ShaderDef {
		std::string name{};						  ///< shader name as written in the script
		std::string file{};						  ///< script file it is from
		std::string texture{};					  ///< image of the first image stage (map, clampMap, animMap, implicit*), "" if none
		std::vector<std::string> animFrames{};	  ///< animMap images (texture is the first one)
		float animFrequency = 0.0f;				  ///< animMap frames per second
		bool clamp = false;						  ///< clampMap
		bool environment = false;				  ///< the stage has "tcGen environment"
		std::string editorImage{};				  ///< qer_editorimage
		ShaderCull cull = ShaderCull::Front;	  ///< sides that are drawn
		AlphaFunc alphaFunc = AlphaFunc::None;	  ///< alpha test of the image stage
		bool blend = false;						  ///< the image stage blends with blendFunc
		BlendFactor blendSrc = BlendFactor::One;  ///< blend source factor
		BlendFactor blendDst = BlendFactor::Zero; ///< blend destination factor
		bool depthWrite = true;					  ///< writes the depth (blended stages do not unless depthWrite is set)
		bool noPicmip = false;					  ///< nopicmip
		std::vector<std::string> surfaceParms{};  ///< surfaceparm values
		int stages = 0;							  ///< number of stages

		/**
		 * @return the image to show: the stage image, else the editor image
		 */
		[[nodiscard]] std::string image() const
		{
			return texture.empty() ? editorImage : texture;
		}
	};

	/**
	 * Key of a shader or material name like the game compares them: lowercase, forward slashes, without the extension
	 * (a material "models/foo.tga" uses the shader "models/foo")
	 * @param name shader or material name
	 * @return lookup key
	 */
	std::string shaderKey(const std::string &name);

	/**
	 * Parse a shader script. Unknown keywords are skipped, a broken shader ends the parsing of the file (like the game,
	 * which stops at errors) but keeps the shaders before it.
	 * @param text script text
	 * @param file file name stored in the shaders
	 * @return the shaders in the order of the file
	 */
	std::vector<ShaderDef> parseShaderScript(const std::string &text, const std::string &file = "");

	/*! The shaders of every script, looked up by material name */
	class ShaderLibrary {
	  public:
		/**
		 * Load every .shader file in scripts/ of a file system. Like the game, a shader in a file that sorts later
		 * replaces one with the same name in an earlier file; inside a file the first one counts.
		 * @param files file system
		 * @return number of shader files read
		 */
		size_t load(mt::io::FileSystem &files);

		/**
		 * Add the shaders of a script, replacing the ones with the same name
		 * @param shaders parsed shaders
		 */
		void add(const std::vector<ShaderDef> &shaders);

		/**
		 * @param material material or shader name (case, slashes and the extension are ignored)
		 * @return the shader, nullptr if there is none
		 */
		[[nodiscard]] const ShaderDef *find(const std::string &material) const;

		/**
		 * @return number of shaders
		 */
		[[nodiscard]] size_t size() const
		{
			return shaders.size();
		}

		void clear()
		{
			shaders.clear();
		}

	  private:
		std::unordered_map<std::string, ShaderDef> shaders{};
	};

	/**
	 * Find the texture of a material like the game does: the image of the shader with the material's name, else the
	 * image named like the material (see io::findTexture)
	 * @param source file source to search
	 * @param shaders loaded shaders
	 * @param material material name of a mesh
	 * @param modelPath path of the model, images are also looked up next to it
	 * @return path of the texture, empty if not found
	 */
	std::string findMaterialTexture(mt::io::FileSource &source, const ShaderLibrary &shaders, const std::string &material,
									const std::string &modelPath);
}
