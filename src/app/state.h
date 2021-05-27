#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"

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

	private:
		State() = default;
	};
}
