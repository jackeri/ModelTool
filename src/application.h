#pragma once

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
	};
}
