#include "ScenePanel.h"
#include "Framebuffer.h"
#include "Camera.h"
#include "imgui.h"

mt::Framebuffer framebuffer;

mt::ScenePanel::ScenePanel()
{
}

mt::ScenePanel::~ScenePanel()
{

}
namespace mt {
	static void gl1Test(Camera &cam)
	{
		glLoadIdentity();
		// glTranslatef(0,0,1.f);

		glTranslatef(cam.xPos, cam.yPos, cam.zPos);
		glScalef(cam.scaleFactor, cam.scaleFactor, cam.scaleFactor);
		glRotatef(cam.rotAngleX, 1.0f, 0.0f, 0.0f);
		glRotatef(cam.rotAngleY, 0.0f, 1.0f, 0.0f);
		glRotatef(cam.rotAngleZ, 1.0f, 0.0f, 0.0f);

		const float ORIGIN_LINE_LEN = 60.0f;
		const float GRID_LINE_LEN = ORIGIN_LINE_LEN * 2;
		const byte ORIGIN_TEXT_COLOUR[] = {255, 0, 255};

		const byte xAxis[] = { 255,0,0 };
		const byte yAxis[] = { 0,255,0 };
		const byte zAxis[] = { 0,0,255 };

		byte origColor[] = {255, 0, 255};

		static float v3X[] = {ORIGIN_LINE_LEN, 0, 0};
		static float v3XNeg[] = {-ORIGIN_LINE_LEN, 0, 0};

		static float v3Y[] = {0, ORIGIN_LINE_LEN, 0};
		static float v3YNeg[] = {0, -ORIGIN_LINE_LEN, 0};

		static float v3Z[] = {0, 0, ORIGIN_LINE_LEN};
		static float v3ZNeg[] = {0, 0, -ORIGIN_LINE_LEN};

		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
		{
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_LIGHTING);

			//Draw the grid first
			glColor4f(0, 0, 0, 0.5f);
			glBegin(GL_LINES);
			for (float i = 0; i <= GRID_LINE_LEN; i += 2.0f)
			{
				glVertex3f(i - ORIGIN_LINE_LEN, -ORIGIN_LINE_LEN, 0.0f);
				glVertex3f(i - ORIGIN_LINE_LEN, ORIGIN_LINE_LEN, 0.0f);
				glVertex3f(-ORIGIN_LINE_LEN, i - ORIGIN_LINE_LEN, 0.0f);
				glVertex3f(ORIGIN_LINE_LEN, i - ORIGIN_LINE_LEN, 0.0f);
			}
			glEnd();


			//Draw the lines
			//glColor3f(0, 0, 1);
			glLineWidth(2);
			glBegin(GL_LINES);
			{
				glColor3ubv(xAxis);
				glVertex3fv(v3X);
				glVertex3fv(v3XNeg);

				glColor3ubv(yAxis);
				glVertex3fv(v3Y);
				glVertex3fv(v3YNeg);

				glColor3ubv(zAxis);
				glVertex3fv(v3Z);
				glVertex3fv(v3ZNeg);
			}
			glEnd();
			glLineWidth(1);
		}
		glPopAttrib();
	}
}


void mt::ScenePanel::render()
{
	ImGuiWindowFlags sceneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;

	ImGui::Begin("Scene", nullptr, sceneFlags);

	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	size = {viewportPanelSize.x, viewportPanelSize.y};

	framebuffer.create(size.x, size.y);
	framebuffer.bind();

	ImColor clearColor = {114, 144, 154};

	glClearColor(clearColor.Value.x, clearColor.Value.y, clearColor.Value.z, clearColor.Value.w);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (size.y > 0)
	{
		gluPerspective(90.f, (double)size.x / (double)size.y, 0.1f, 512);
	}
	glViewport(0, 0, size.x, size.y);

	glMatrixMode(GL_MODELVIEW);

	gl1Test(camera);

	framebuffer.unbind();

	GLuint textureID = framebuffer.texture();
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
