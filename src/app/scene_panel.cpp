#include "scene_panel.h"
#include "framebuffer.h"
#include "camera.h"
#include "glrenderer2.h"
#include "state.h"

mt::ScenePanel::ScenePanel()
{
#if defined(MT_OPENGL_2)
	renderer = make_ref<GLRenderer2>();
#elif defined(MT_OPENGL_3)
#error Renderer implementation for Opengl3 missing
#endif
	fbo = make_ref<Framebuffer>();
}

mt::ScenePanel::~ScenePanel()
{

}

void mt::ScenePanel::render()
{
	ImGuiWindowFlags sceneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
	auto &state = singleton<State>();

	ImGui::Begin("Scene", nullptr, sceneFlags);

	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	size = {viewportPanelSize.x, viewportPanelSize.y};

	fbo->create(size.x, size.y);
	fbo->bind();

	renderer->startFrame(camera);

	renderer->setView(size.x, size.y);

	renderer->grid();

	if (state.model)
	{
		state.model->renderModel(renderer);
	}

	renderer->endFrame();

	fbo->unbind();

	GLuint textureID = fbo->texture();
	auto imVec = ImVec2{size.x, size.y};
	ImGui::Image(reinterpret_cast<void *>(textureID), imVec, ImVec2{0, 1}, ImVec2{1, 0});
	if (ImGui::IsItemHovered())
	{
		sceneHovered = true;
		if (ImGui::IsMouseClicked(0))
		{
			captureMouse = true;
		}
	}
	else
	{
		sceneHovered = false;
	}

	if (sceneHovered && ImGui::GetIO().MouseWheel != 0)
	{
		if (ImGui::GetIO().MouseWheel > 0)
		{
			camera.zPos += CAMERA_ZOOM_STEP;
		}
		else
		{
			camera.zPos -= CAMERA_ZOOM_STEP;
		}

		camera.checkLimits(true);
	}

	ImGui::End();
}

void mt::ScenePanel::onMouseMove(double x, double y, int button)
{
	if (!captureMouse)
	{
		return;
	}

	if (button != 1)
	{
		captureMouse = false;
		return;
	}

	auto delta = ImGui::GetIO().MouseDelta;
	camera.rotAngleY += (float) (delta.x) * CAMERA_ROTATION_SCALE;
	camera.rotAngleX += (float) (delta.y) * CAMERA_ROTATION_SCALE;
	camera.checkLimits();
}
