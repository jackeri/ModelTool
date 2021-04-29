#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"

namespace mt {

	class State {
	public:
		std::vector<Model> models{};
		ImColor clearColor{114, 144, 154};
		IO::FileSystem filesystem{};

		State() = default;

		~State()
		{
			clear();
		}

		void clear()
		{
			emptyVector(models);
		}
	};
}
