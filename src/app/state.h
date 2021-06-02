#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"
#include "ModelLoader.h"

namespace mt {

	class State {

		friend Ref<State> singleton_ref<State>();

	public:
		std::unique_ptr<Model> model = nullptr;
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
			closeModels();
		}

		void loadModel(const std::string &path)
		{
			closeModels();

			auto *tmp = model::ModelLoader::loadModel(filesystem.loadFile(path));
			if (tmp)
			{
				model = std::unique_ptr<Model>(tmp);
			}
		}

		void loadAnimation(const std::string &path)
		{
			if (!model)
			{
				return;
			}

			model::ModelLoader::loadAnimation(model.get(), filesystem.loadFile(path));
		}

		void closeModels()
		{
			model = nullptr;
		}

	private:
		State() = default;
	};
}
