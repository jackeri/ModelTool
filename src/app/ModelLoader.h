#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"
#include <unordered_map>

namespace mt::model {
	using ModelLoaderMap = std::unordered_map<std::string, const std::function<Model *(const Ref<IO::MTFile> &)>>;
	using AnimationLoaderMap = std::unordered_map<std::string, const std::function<void(Model *,
																						const Ref<IO::MTFile> &)>>;

	class ModelLoader {

		static Model *loadModel(const Ref<IO::MTFile> &);

		static Ref<Model> loadModel_ref(const Ref<IO::MTFile> &);

		static void loadAnimation(Model *, const Ref<IO::MTFile> &);

		static void loadAnimation(const Ref<Model>&, const Ref<IO::MTFile> &);

	private:
		ModelLoader() = default;

		static void init();

		static ModelLoaderMap m_modelLoaders;
		static AnimationLoaderMap m_animationLoaders;
	};

	ModelLoaderMap ModelLoader::m_modelLoaders{};
	AnimationLoaderMap ModelLoader::m_animationLoaders{};
}
