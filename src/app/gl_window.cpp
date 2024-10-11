#include "gl_window.h"
#include "imgui_view.h"
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

mt::GLWindow::GLWindow(std::string title) : title(std::move(title)) {}

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

#if defined(MT_OPENGL_2)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#elif defined(MT_OPENGL_3)
	// Let's use OpenGL 3.3 for now, maybe upgrade in the future
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
#error What GL version are we supposed to use?
#endif

	GLFWwindow *glfwWindow = glfwCreateWindow(1280, 720, title.c_str(), nullptr, nullptr);
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
	scenePanel = make_ref<ScenePanel>();

	view = make_ref<ImGuiView>();

	// Init the UI system
	if (!view->setup(this, "#version 150"))
	{
		logger->error("Failed to initialize ImGui");
		// UI setup failed, cancel the whole ordeal.
		return false;
	}

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

		view->startFrame();

		view->draw();

		propertyPanel->render();

		scenePanel->render();

		view->endFrame();

		handleInput();

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

void mt::GLWindow::handleInput()
{
	double x, y;
	int mouseButton = 0;
	glfwGetCursorPos(window, &x, &y);

	// Left mouse button
	if (glfwGetMouseButton(window, 0) == GLFW_PRESS)
	{
		mouseButton = 1;
	}
	// Right mouse button
	else if (glfwGetMouseButton(window, 1) == GLFW_PRESS)
	{
		mouseButton = 2;
	}
	// Middle mouse button
	else if (glfwGetMouseButton(window, 2) == GLFW_PRESS)
	{
		mouseButton = 3;
	}

	scenePanel->onMouseMove(x, y, mouseButton);
}
