#pragma once

#include "library.h"
#include "files.h"
#include "measure.h"
#include "model.h"

namespace mt::model {
	struct Skin;
	class ShaderLibrary;

	/*! A model in a glTF export: the main model, or a model attached to a tag or bone of another part */
	struct GltfPart {
		const Model *model = nullptr; ///< the model
		std::string name{};			  ///< node name of the part
		std::string path{};			  ///< model file, textures are also looked up next to it
		int parent = -1;			  ///< part it is attached to, -1 for the main model (parts come after their parent)
		std::string attachPoint{};	  ///< tag or bone of the parent part
		const Skin *skin = nullptr;	  ///< skin of the part, its shaders replace the mesh materials
	};

	/*! How a glTF file is written */
	struct GltfOptions {
		bool binary = true;								 ///< .glb, else .gltf with the buffer embedded as base64
		float scale = METERS_PER_UNIT;					 ///< meters per game unit (1 unit = 1 inch), 1 keeps game units
		bool morphTargets = true;						 ///< MD3 / MDC frames as morph targets, else the first export frame only
		mt::io::FileSource *files = nullptr;			 ///< embed the textures found here (nullptr: material names only)
		const ShaderLibrary *shaders = nullptr;			 ///< shaders for the textures and the material settings
		std::function<void(const std::string &)> warn{}; ///< called for problems that do not stop the export
	};

	/**
	 * Write models as glTF 2.0. The scene has a root node that turns the idTech Z-up coordinates into glTF's Y-up (the
	 * reverse of the glTF import) and scales them to meters. Each part is a node with its model transform; attached parts
	 * hang on their parent's tag or bone node, so they follow its animation.
	 *
	 * Skeletal models: the bones are a skin (the bind pose, else the first export frame, is the rest pose; at most 4
	 * weights per vertex, the strongest), every animation sequence in the export frames is a glTF animation with the
	 * bones' translation and rotation. Morph models (MD3, MDC): the export frames are morph targets and the sequences
	 * animate their weights; tags are nodes animated with the frames. Materials take the skin's shader, the image of the
	 * shader (embedded as PNG, or as JPEG when it is one), double sided for "cull none", alpha mask / blend like the
	 * shader. Triangles are turned to glTF's counter-clockwise winding.
	 * @param parts the models, the first one is the main model
	 * @param options how to write
	 * @return the file data
	 * @throws mt::mt_exception if a part has no model or an invalid parent
	 */
	std::vector<byte> encodeGLTF(const std::vector<GltfPart> &parts, const GltfOptions &options);

	/**
	 * Write one model as .glb (or .gltf by the extension) without textures, for the writer registry
	 * @param model model to write
	 * @param path output file
	 */
	void writeGLTF(const Model &model, const std::string &path);

	/**
	 * Write models as .glb or .gltf (by the extension of the path)
	 * @param parts the models, the first one is the main model
	 * @param path output file
	 * @param options how to write, binary is set from the extension
	 */
	void writeGLTF(const std::vector<GltfPart> &parts, const std::string &path, GltfOptions options);
}
