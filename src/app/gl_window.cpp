#include "gl_window.h"
#include "imgui_view.h"
#include "state.h"
#include <map>
#include <utility>

static void glfwAppErrorCallback(int error, const char *description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void on_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	auto pWindow = static_cast<mt::GLWindow *>(glfwGetWindowUserPointer(window));
	pWindow->onKey(key, scancode, action, mods);
}

static void on_scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
	auto pWindow = static_cast<mt::GLWindow *>(glfwGetWindowUserPointer(window));
	pWindow->onScroll(yoffset);
}

static void on_window_size_callback(GLFWwindow *window, int width, int height)
{
	auto pWindow = static_cast<mt::GLWindow *>(glfwGetWindowUserPointer(window));
	pWindow->onResize(width, height);
}

mt::GLWindow::GLWindow(std::string title, AppOptions options) : title(std::move(title)), options(std::move(options)) {}

mt::GLWindow::~GLWindow()
{
	destroy();
	glfwTerminate();
}

void mt::GLWindow::destroy()
{
	if (view)
	{
		view->shutdown();
		view = nullptr;
	}

	if (window)
	{
		glfwDestroyWindow(window);
		window = nullptr;
	}
}

bool mt::GLWindow::init()
{
	auto logger = spdlog::get(MT_LOGGER);

	glfwSetErrorCallback(glfwAppErrorCallback);
	if (!glfwInit())
	{
		logger->error("Failed to initialize GLFW");
		return false;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);

	// OpenGL 4.1 core, the newest version macOS supports
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Screenshots are rendered off screen
	glfwWindowHint(GLFW_VISIBLE, options.screenshot.empty() ? GLFW_TRUE : GLFW_FALSE);

	const bool sizeFromOptions = !options.screenshot.empty() && options.screenshotUi;
	GLFWwindow *glfwWindow
		= glfwCreateWindow(sizeFromOptions ? options.width : 1280, sizeFromOptions ? options.height : 720, title.c_str(), nullptr, nullptr);
	if (glfwWindow == nullptr)
	{
		logger->error("Failed to create GLFW window");
		return false;
	}
	this->window = glfwWindow;

	glfwMakeContextCurrent(glfwWindow);
	glfwSwapInterval(1);

	glfwSetWindowUserPointer(glfwWindow, this);
	glfwSetKeyCallback(glfwWindow, on_key_callback);
	glfwSetScrollCallback(glfwWindow, on_scroll_callback);
	glfwSetWindowSizeCallback(glfwWindow, on_window_size_callback);

	int gladVersion = gladLoadGL(glfwGetProcAddress);
	if (gladVersion == 0)
	{
		logger->error("Failed to initialize OpenGL context with Glad");
		return false;
	}

	// Print the current GPU info
	const GLubyte *rendererStr = glGetString(GL_RENDERER);
	const GLubyte *version = glGetString(GL_VERSION);

	logger->info("Renderer: {}", reinterpret_cast<const char *>(rendererStr));
	logger->info("OpenGL version supported: {}", reinterpret_cast<const char *>(version));
	logger->debug("Loaded OpenGL according to Glad {}.{}", GLAD_VERSION_MAJOR(gladVersion), GLAD_VERSION_MINOR(gladVersion));

	propertyPanel = make_ref<ModelPropertyPanel>();
	try
	{
		scenePanel = make_ref<ScenePanel>();
	}
	catch (const std::exception &ex)
	{
		logger->error("Failed to create the renderer: {}", ex.what());
		return false;
	}

	view = make_ref<ImGuiView>();

	// Init the UI system
	if (!view->setup(this, "#version 410"))
	{
		logger->error("Failed to initialize ImGui");
		// UI setup failed, cancel the whole ordeal.
		return false;
	}

	view->setScene(scenePanel.get());
	applyOptions();

	return true;
}

GLFWwindow *mt::GLWindow::system()
{
	return window;
}

void mt::GLWindow::loop()
{
	auto *glfwWindow = static_cast<GLFWwindow *>(this->window);
	while (!glfwWindowShouldClose(glfwWindow))
	{
		glfwPollEvents();

		// Close the window on escape for now (faster dev)
		if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(glfwWindow, true);
		}

		// Clear the screen for now
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		singleton<State>().update(ImGui::GetIO().DeltaTime);

		view->startFrame();

		view->draw();

		propertyPanel->render();

		scenePanel->render();

		view->endFrame();

		glfwSwapBuffers(glfwWindow);
	}
}

void mt::GLWindow::onKey(int key, int scancode, int action, int mods)
{
	// FIXME: is this needed or not anymore
}

void mt::GLWindow::onScroll(double delta)
{
	// FIXME: is this needed or not anymore
}

void mt::GLWindow::onResize(int width, int height)
{
	// FIXME: is this needed or not anymore
}

void mt::GLWindow::applyOptions()
{
	auto logger = spdlog::get(MT_LOGGER);
	auto &state = singleton<State>();

	for (const auto &path : options.basePaths)
	{
		if (!state.filesystem.addPath(path))
		{
			logger->error("Invalid base path: {}", path);
		}
	}

	if (!options.model.empty())
	{
		try
		{
			state.loadModel(options.model);
			for (const auto &animation : options.animations)
			{
				state.loadAnimation(animation);
			}

			if (state.model && (options.frame >= 0 || !state.model->skeletalModel()))
			{
				state.model->currentFrame = std::clamp(options.frame, 0, std::max(state.model->numFrames() - 1, 0));
			}
		}
		catch (const std::exception &ex)
		{
			logger->error("Failed to load {}: {}", options.model, ex.what());
		}
	}

	auto &view = state.view;
	const std::map<std::string, std::pair<bool *, bool>> toggles{
		{"wireframe", {&view.wireframe, true}},	  {"notextures", {&view.textures, false}},	{"alpha", {&view.alpha, true}},
		{"nogrid", {&view.drawGrid, false}},	  {"bounds", {&view.drawBounds, true}},		{"nohitboxes", {&view.drawHitboxes, false}},
		{"tags", {&view.drawTags, true}},		  {"skeleton", {&view.drawSkeleton, true}}, {"normals", {&view.drawNormals, true}},
		{"dynamiclod", {&view.dynamicLod, true}},
	};

	for (const auto &item : options.view)
	{
		auto toggle = toggles.find(item);
		if (toggle == toggles.end())
		{
			logger->warn("Unknown view option: {}", item);
			continue;
		}
		*toggle->second.first = toggle->second.second;
	}
}

bool mt::GLWindow::screenshot()
{
	bool ok;
	if (options.screenshotUi)
	{
		// A few frames so the docking layout and the windows settle
		for (int frame = 0; frame < 10; frame++)
		{
			glfwPollEvents();
			view->startFrame();
			view->draw();
			propertyPanel->render();
			scenePanel->render();
			view->endFrame();
		}

		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		std::vector<byte> pixels(size_t(width) * size_t(height) * 4);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glReadBuffer(GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		ok = ScenePanel::writePng(options.screenshot, width, height, pixels);
	}
	else
	{
		scenePanel->renderScene(options.width, options.height);
		ok = scenePanel->saveImage(options.screenshot);
	}
	auto logger = spdlog::get(MT_LOGGER);
	if (ok)
	{
		logger->info("Saved screenshot: {}", options.screenshot);
	}
	else
	{
		logger->error("Failed to save screenshot: {}", options.screenshot);
	}
	return ok;
}
