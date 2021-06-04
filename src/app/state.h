#pragma once

#include "mt.h"
#include "model.h"
#include "filesystem.h"
#include "modelloader.h"

namespace mt {

	/*! State singleton object */
	class State {

		friend Ref<State> singleton_ref<State>();

	public:
		std::unique_ptr<Model> model = nullptr; ///< currently loaded model
		ImColor clearColor{114, 144, 154}; ///< background clear color
		IO::FileSystem filesystem{}; ///< used filesystem, which will be initialized by the ImGuiView

		/**
		 * no copy for singletons
		 */
		State(State const &) = delete;

		/**
		 * No assign for singletons
		 * @return delete
		 */
		State &operator=(State const &) = delete;

		~State()
		{
			clear();
		}

		/**
		 * Clear all values and models (possibly to have linked models in the future)
		 */
		void clear()
		{
			closeModels();
		}

		/**
		 * Uses the filesystem and the modelloader to load a model into the current state
		 * @param path path to the model
		 */
		void loadModel(const std::string &path)
		{
			closeModels();

			auto *tmp = model::ModelLoader::loadModel(filesystem.loadFile(path));
			if (tmp)
			{
				model = std::unique_ptr<Model>(tmp);
			}
		}

		/**
		 * Uses the filesystem and the modelloader to load an animation into the current state's model
		 * @param path path to the animation
		 */
		void loadAnimation(const std::string &path)
		{
			if (!model)
			{
				return;
			}

			model::ModelLoader::loadAnimation(model.get(), filesystem.loadFile(path));
		}

		/**
		 * Clear models (possibly to have linked models in the future)
		 */
		void closeModels()
		{
			model = nullptr;
		}

	private:

		/**
		 * Hidden constructor for singleton (the friend function handles it)
		 */
		State() = default;
	};
}
