#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"
#include "ModelLoader.h"

namespace mt {

	class State {

		friend Ref<State> singleton_ref<State>();

	public:
		Model *model = nullptr;
		std::vector<Ref<Model>> models{};
		ImColor clearColor{114, 144, 154};
		IO::FileSystem filesystem{};

		State(State const &) = delete;

		State &operator=(State const &) = delete;

		~State()
		{
			clear();
		}

		void clear()
		{
			emptyVector(models);
		}

		void loadModel(const std::string &path)
		{
			closeModels();
			model = model::ModelLoader::loadModel(filesystem.loadFile(path));
		}

		void loadAnimation(const std::string &path)
		{
			if (!model)
			{
				return;
			}

			model::ModelLoader::loadAnimation(model, filesystem.loadFile(path));
		}

		void closeModels()
		{
			delete model;
			model = nullptr;
		}

	private:
		State() = default;
	};
}
