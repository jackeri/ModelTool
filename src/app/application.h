#pragma once

#include "mt.h"
#include "GLWindow.h"

namespace mt {
	class Application {
	public:
		explicit Application(const std::string &title);

		~Application();

		bool init();

		void loop();

	private:
		Ref<GLWindow> window = nullptr;
	};
}
