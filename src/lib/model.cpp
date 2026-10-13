#include "model.h"
#include "model_gltf.h"
#include "model_iqm.h"
#include "model_md3.h"
#include "model_md5.h"
#include "model_mdm.h"
#include "model_ms3d.h"
#include "model_mdx.h"
#include "model_ufbx.h"
#include "model_scripts.h"
#include "model_stl.h"

#include <filesystem>
#include <map>

namespace mt::model::io {

	template<typename T>
	using Registry = std::map<std::string, T>;

	// Function local statics so the registries exist before any static registration code runs
	// clang-format off
	static Registry<ModelLoader> &modelLoaders()
	{
		static Registry<ModelLoader> loaders{
			{".fbx", model::loadFBX},
			{".glb", model::loadGLTF},
			{".gltf", model::loadGLTF},
			{".iqm", model::loadIQM},
			{".md3", model::loadMD3},
			{".md5mesh", model::loadMd5Model},
			{".mdc", model::loadMDC},
			{".mdm", model::loadMDM},
			{".mds", model::loadMDS},
			{".mdx", model::loadMDX},
			{".ms3d", model::loadMS3D},
			{".obj", model::loadOBJ},
		};
		return loaders;
	}

	static Registry<AnimationLoader> &animationLoaders()
	{
		static Registry<AnimationLoader> loaders{
			{".aninc", model::loadAninc},
			{".md5anim", model::loadMD5Anim},
			{".mdx", model::loadMDXAnimation},
			{".ms3d", model::loadMS3DAnimation},
		};
		return loaders;
	}
	// clang-format on

	// clang-format off
	static Registry<ModelWriter> &modelWriters()
	{
		static Registry<ModelWriter> writers{
			{".md3", model::writeMD3},
			{".md5mesh", model::writeMD5Mesh},
			{".mdc", [](const Model &model, const std::string &path) { model::writeMDC(model, path); }},
			{".mdm", model::writeMDM},
			{".mds", model::writeMDS},
			{".mdx", model::writeMDX},
			{".ms3d", model::writeMS3D},
			{".stl", [](const Model &model, const std::string &path) { model::writeSTL(model, path, true); }},
			{".tag", model::writeTags},
		};
		return writers;
	}

	static Registry<AnimationWriter> &animationWriters()
	{
		static Registry<AnimationWriter> writers{
			{".aninc", model::writeAninc},
			{".md5anim", model::writeMD5Anim},
			{".mdx", model::writeMDX},
		};
		return writers;
	}
	// clang-format on

	template<typename T>
	static const T &find(const Registry<T> &registry, const std::string &ext, const std::string &what)
	{
		auto it = registry.find(ext);
		if (it == registry.end())
		{
			throw mt_ex("No " + what + " for extension: '" + ext + "'");
		}
		return it->second;
	}

	template<typename T>
	static std::vector<std::string> keys(const Registry<T> &registry)
	{
		std::vector<std::string> list;
		list.reserve(registry.size());
		for (const auto &entry : registry)
		{
			list.push_back(entry.first);
		}
		return list;
	}

	std::string normalizeExtension(const std::string &extOrName)
	{
		std::string ext = extOrName;

		// A path or a file name, take the extension part
		if (ext.find('/') != std::string::npos || ext.find('\\') != std::string::npos || ext.find('.', 1) != std::string::npos)
		{
			ext = std::filesystem::path(ext).extension().string();
		}

		if (!ext.empty() && ext[0] != '.')
		{
			ext.insert(ext.begin(), '.');
		}

		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		return ext;
	}

	static std::string fileExtension(const Ref<mt::io::MTFile> &file)
	{
		if (!file)
		{
			throw mt_ex("File not found");
		}

		return normalizeExtension(file->ext.empty() ? file->name : file->ext);
	}

	Model *loadModel(const Ref<mt::io::MTFile> &file)
	{
		return find(modelLoaders(), fileExtension(file), "model loader")(file);
	}

	void loadAnimation(Model *parent, const Ref<mt::io::MTFile> &file)
	{
		if (!parent)
		{
			throw mt_ex("No model to load the animation into");
		}

		find(animationLoaders(), fileExtension(file), "animation loader")(parent, file);
	}

	void writeModel(const Model &model, const std::string &path)
	{
		find(modelWriters(), normalizeExtension(path), "model writer")(model, path);
	}

	void writeAnimation(const Model &model, const std::string &path)
	{
		find(animationWriters(), normalizeExtension(path), "animation writer")(model, path);
	}

	void registerModelLoader(const std::string &ext, ModelLoader loader)
	{
		modelLoaders()[normalizeExtension(ext)] = std::move(loader);
	}

	void registerAnimationLoader(const std::string &ext, AnimationLoader loader)
	{
		animationLoaders()[normalizeExtension(ext)] = std::move(loader);
	}

	void registerModelWriter(const std::string &ext, ModelWriter writer)
	{
		modelWriters()[normalizeExtension(ext)] = std::move(writer);
	}

	void registerAnimationWriter(const std::string &ext, AnimationWriter writer)
	{
		animationWriters()[normalizeExtension(ext)] = std::move(writer);
	}

	std::vector<std::string> modelExtensions()
	{
		return keys(modelLoaders());
	}

	std::vector<std::string> animationExtensions()
	{
		return keys(animationLoaders());
	}

	std::vector<std::string> modelWriterExtensions()
	{
		return keys(modelWriters());
	}

	std::vector<std::string> animationWriterExtensions()
	{
		return keys(animationWriters());
	}
}
