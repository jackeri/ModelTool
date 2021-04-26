#pragma once

#include "mt.h"
#include "renderer.h"

namespace mt {
	class Application {

	public:
		bool init();
		void loop();

		explicit Application(const std::string& title);
		~Application();

	private:
		std::string title;
		void *window = nullptr;
		Ref<Renderer> renderer = nullptr;
	};
}
