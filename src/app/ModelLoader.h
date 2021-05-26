#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"
#include <unordered_map>

namespace mt::model {
	using LoaderMap = std::unordered_map<std::string, const std::function<Model *(const Ref<IO::MTFile> &)>>;

	class ModelLoader {

		static Model *load(const Ref<IO::MTFile> &);

		static Ref<Model> load_ref(const Ref<IO::MTFile> &);

	private:
		ModelLoader() = default;

		static void init();

		static LoaderMap m_loaders;
	};

	LoaderMap ModelLoader::m_loaders{};
}
