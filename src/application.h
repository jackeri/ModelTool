#ifndef MODELTOOL_APPLICATION_H
#define MODELTOOL_APPLICATION_H

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
#endif //MODELTOOL_APPLICATION_H
