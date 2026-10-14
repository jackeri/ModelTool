#include "gl_window.h"
#include "app_icon.h"
#include "hitbox.h"
#include "imgui_view.h"
#include "state.h"
#include "app_settings.h"
#include "settings.h"
#include <map>
#include <utility>

static void glfwAppErrorCallback(int error, const char *description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
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

	// Keep the working folder: GLFW moves into the bundle's Resources folder by default, which breaks relative paths
	// given on the command line
	glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);

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

	// Screenshots are rendered off screen and do not use the settings
	glfwWindowHint(GLFW_VISIBLE, options.screenshot.empty() ? GLFW_TRUE : GLFW_FALSE);

	auto &state = singleton<State>();
	state.persistSettings = options.screenshot.empty() && !settingsFolder().empty();
	if (state.persistSettings)
	{
		settingsPath = settingsFolder() + "/settings.cfg";
		state.settings = AppSettings::load(settingsPath);
		logger->info("Settings: {}", settingsPath);
	}

	glm::ivec2 windowSize = state.persistSettings ? state.settings.windowSize : glm::ivec2{1280, 720};
	if (!options.screenshot.empty() && options.screenshotUi)
	{
		windowSize = {options.width, options.height};
	}
	GLFWwindow *glfwWindow = glfwCreateWindow(windowSize.x, windowSize.y, title.c_str(), nullptr, nullptr);
	if (glfwWindow == nullptr)
	{
		logger->error("Failed to create GLFW window");
		return false;
	}
	this->window = glfwWindow;
	setAppIcon(glfwWindow);

	if (state.persistSettings)
	{
		if (state.settings.windowPosition.x >= 0 && state.settings.windowPosition.y >= 0)
		{
			glfwSetWindowPos(glfwWindow, state.settings.windowPosition.x, state.settings.windowPosition.y);
		}
		if (state.settings.windowMaximized)
		{
			glfwMaximizeWindow(glfwWindow);
		}
	}

	glfwMakeContextCurrent(glfwWindow);
	glfwSwapInterval(state.settings.vsync ? 1 : 0);

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

	if (state.persistSettings)
	{
		applySettings(state, scenePanel->getCamera());

		// The base folders of the last run, unless the command line gives them
		if (options.basePaths.empty())
		{
			for (const auto &path : state.settings.basePaths)
			{
				if (!state.addBasePath(path))
				{
					logger->warn("Base folder not found: {}", path);
				}
			}
		}
	}

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
	int frame = 0;
	while (!glfwWindowShouldClose(glfwWindow))
	{
		if (options.frames > 0 && ++frame > options.frames)
		{
			glfwSetWindowShouldClose(glfwWindow, GLFW_TRUE);
		}

		glfwPollEvents();

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

	saveSettings();
}

void mt::GLWindow::saveSettings()
{
	auto &state = singleton<State>();
	if (!state.persistSettings || !window)
	{
		return;
	}

	state.settings.windowMaximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
	if (!state.settings.windowMaximized)
	{
		glfwGetWindowPos(window, &state.settings.windowPosition.x, &state.settings.windowPosition.y);
		glfwGetWindowSize(window, &state.settings.windowSize.x, &state.settings.windowSize.y);
	}
	collectSettings(state, scenePanel->getCamera());

	auto logger = spdlog::get(MT_LOGGER);
	try
	{
		if (!state.settings.save(settingsPath))
		{
			logger->error("Could not save the settings: {}", settingsPath);
		}
	}
	catch (const std::exception &ex)
	{
		logger->error("Could not save the settings: {}", ex.what());
	}
}

void mt::GLWindow::applyOptions()
{
	auto logger = spdlog::get(MT_LOGGER);
	auto &state = singleton<State>();

	for (const auto &path : options.basePaths)
	{
		if (!state.addBasePath(path))
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
			if (!options.hitboxes.empty() && state.model && state.model->getHitboxes())
			{
				model::Hitbox::loadHitboxes(state.filesystem.loadFile(options.hitboxes), *state.model->getHitboxes());
			}

			if (!options.skin.empty() && state.isPlayer)
			{
				state.setPlayerSkin(options.skin);
			}
			for (const auto &attachment : options.attachments)
			{
				const auto split = attachment.find('=');
				state.attachModel(attachment.substr(split + 1), -1, attachment.substr(0, split));
			}
			for (const auto &name : options.sequences)
			{
				auto play = [&](int target, const std::vector<model::Animation> &list) {
					for (const auto &animation : list)
					{
						if (animation.name == name)
						{
							state.playSequence(target, animation);
						}
					}
				};
				play(-1, state.model->animations);
				for (int i = 0; i < int(state.attachments.size()); i++)
				{
					play(i, state.attachments[i].model->animations);
				}
			}

			if (state.model && (options.frame >= 0 || (!state.model->skeletalModel() && !state.isPlayer)))
			{
				state.model->currentFrame = std::clamp(options.frame, 0, std::max(state.model->numFrames() - 1, 0));
			}
		}
		catch (const std::exception &ex)
		{
			logger->error("Failed to load {}: {}", options.model, ex.what());
		}
	}

	for (const auto &path : options.exports)
	{
		if (!state.model)
		{
			logger->error("Nothing to export to {}", path);
			continue;
		}

		try
		{
			// Animation only formats (.md5anim, .aninc) use the animation writers, the rest the model writers
			auto ext = model::io::normalizeExtension(path);
			auto modelFormats = model::io::modelWriterExtensions();
			if (std::find(modelFormats.begin(), modelFormats.end(), ext) == modelFormats.end())
			{
				model::io::writeAnimation(*state.model, path);
			}
			else
			{
				model::io::writeModel(*state.model, path);
			}
			logger->info("Exported {}", path);
		}
		catch (const std::exception &ex)
		{
			logger->error("Export to {} failed: {}", path, ex.what());
		}
	}

	auto &view = state.view;
	const std::map<std::string, std::pair<bool *, bool>> toggles{
		{"wireframe", {&view.wireframe, true}},		 {"notextures", {&view.textures, false}},  {"alpha", {&view.alpha, true}},
		{"noshaders", {&view.shaders, false}},		 {"twosided", {&view.twoSided, true}},	   {"measure", {&view.drawMeasurements, true}},
		{"flat", {&view.flatShading, true}},		 {"nogrid", {&view.drawGrid, false}},	   {"bounds", {&view.drawBounds, true}},
		{"nohitboxes", {&view.drawHitboxes, false}}, {"tags", {&view.drawTags, true}},		   {"skeleton", {&view.drawSkeleton, true}},
		{"normals", {&view.drawNormals, true}},		 {"dynamiclod", {&view.dynamicLod, true}},
	};

	if (!options.sky.empty())
	{
		state.background = Background::SkyBox;
		state.skyBox = options.sky;
	}
	else if (!options.gradient.empty())
	{
		float r = 0, g = 0, b = 0;
		if (std::sscanf(options.gradient.c_str(), "%f,%f,%f", &r, &g, &b) == 3)
		{
			state.background = Background::Gradient;
			state.backgroundBottom = ImColor(r, g, b);
		}
		else
		{
			logger->warn("Invalid gradient color, expected r,g,b: {}", options.gradient);
		}
	}

	for (const auto &item : options.view)
	{
		// Units of the measurements
		if (item == "metric" || item == "imperial")
		{
			view.lengthUnit = item == "metric" ? model::LengthUnit::Metric : model::LengthUnit::Imperial;
			continue;
		}
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
