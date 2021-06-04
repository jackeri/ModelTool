#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"
#include <unordered_map>

namespace mt::model {
	using ModelLoaderMap = std::unordered_map<std::string, const std::function<Model *(const Ref<IO::MTFile> &)>>;
	using AnimationLoaderMap = std::unordered_map<std::string, const std::function<void(Model *,
																						const Ref<IO::MTFile> &)>>;

	/*! Static loader class for centralized model and animation loading, multiple loaders can be registered for use */
	class ModelLoader {

	public:
		/**
		 * load and parse a model file
		 * @param file file instance to parse
		 * @return Model instance or nullptr if the loading fails
		 */
		static Model *loadModel(const Ref<IO::MTFile> &file);

		/**
		 * load and parse a model file
		 * @param file file instance to parse
		 * @return Model instance of nullptr if the loading fails
		 */
		static Ref<Model> loadModel_ref(const Ref<IO::MTFile> &file);

		/**
		 * load and parse an animation file
		 * @param parent parent model to which to load the animation information
		 * @param file file instance to parse
		 */
		static void loadAnimation(Model *parent, const Ref<IO::MTFile> &file);

		/**
		 * load and parse an animation file
		 * @param parent parent model to which to load the animation information
		 * @param file file instance to parse
		 */
		static void loadAnimation(const Ref<Model> &parent, const Ref<IO::MTFile> &file);

	private:
		ModelLoader() = default;

		/**
		 * Init the model and animation loaders
		 */
		static void init();

		static ModelLoaderMap m_modelLoaders; ///< List of available model loaders with extensions
		static AnimationLoaderMap m_animationLoaders; ///< List of available animation loaders with extensions
	};
}
