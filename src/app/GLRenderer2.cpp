#include "GLRenderer2.h"
#include "Camera.h"
#include "state.h"

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

const mt::byte xAxis[] = {255, 0, 0};
const mt::byte yAxis[] = {0, 255, 0};
const mt::byte zAxis[] = {0, 0, 255};

mt::GLRenderer2::GLRenderer2()
{
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glShadeModel(GL_SMOOTH);

	float mat_specular[] = {1.0, 1.0, 1.0, 1.0};
	float mat_ambient[] = {0.9f, 0.9f, 0.9f, 1.0};
	float mat_diffuse[] = {1, 1, 1, 1};
	float mat_shininess[] = {20.0};

	float light_position[] = {55.0, -50.0, -5.0, 0.0};

	float light2_position[] = {-50.0, 45.0, 15.0, 0.0};
	float mat2_diffuse[] = {0.5f, 0.5f, 1, 1};

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

	glLightfv(GL_LIGHT0, GL_AMBIENT, mat_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, mat2_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glLightfv(GL_LIGHT1, GL_POSITION, light2_position);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, mat2_diffuse);
	glLightfv(GL_LIGHT1, GL_AMBIENT, mat_ambient);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_LIGHTING);

	glEnable(GL_BLEND);

	glBlendFunc(GL_ONE, GL_ZERO); //bug fix

	glEnable(GL_POLYGON_SMOOTH);
}

void mt::GLRenderer2::setView(int width, int height)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (height > 0)
	{
		gluPerspective(90.f, (double)width / (double)height, 0.1f, 512);
	}
	glViewport(0, 0, width, height);
	glMatrixMode(GL_MODELVIEW);
}

void mt::GLRenderer2::push(const glm::mat4x4 &matrix)
{
	glPushMatrix();
	glMultMatrixf(glm::value_ptr(matrix));
}

void mt::GLRenderer2::pop()
{
	glPopMatrix();
}

void mt::GLRenderer2::grid()
{
	glDisable(GL_BLEND);

	//Draw grid
	const float ORIGIN_LINE_LEN = 60.0f;
	const float GRID_LINE_LEN = ORIGIN_LINE_LEN * 2;

	const glm::ivec3 origColor = {255, 0, 255};
	const glm::vec3 v3X = {ORIGIN_LINE_LEN, 0, 0};
	const glm::vec3 v3XNeg = {-ORIGIN_LINE_LEN, 0, 0};
	const glm::vec3 v3Y = {0, ORIGIN_LINE_LEN, 0};
	const glm::vec3 v3YNeg = {0, -ORIGIN_LINE_LEN, 0};
	const glm::vec3 v3Z = {0, 0, ORIGIN_LINE_LEN};
	const glm::vec3 v3ZNeg = {0, 0, -ORIGIN_LINE_LEN};

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
		glLineWidth(2);
		glBegin(GL_LINES);
		{
			glColor3ubv(xAxis);
			glVertex3fv(glm::value_ptr(v3X));
			glVertex3fv(glm::value_ptr(v3XNeg));

			glColor3ubv(yAxis);
			glVertex3fv(glm::value_ptr(v3Y));
			glVertex3fv(glm::value_ptr(v3YNeg));

			glColor3ubv(zAxis);
			glVertex3fv(glm::value_ptr(v3Z));
			glVertex3fv(glm::value_ptr(v3ZNeg));
		}
		glEnd();
		glLineWidth(1);
	}
	glPopAttrib();
}

void mt::GLRenderer2::startFrame(Camera &cam)
{
	auto &color = singleton<State>().clearColor;

	glClearColor(color.Value.x, color.Value.y, color.Value.z, color.Value.w);
	glClear(GL_COLOR_BUFFER_BIT);

	glLoadIdentity();
	glTranslatef(cam.xPos, cam.yPos, cam.zPos);
	glScalef(cam.scaleFactor, cam.scaleFactor, cam.scaleFactor);
	glRotatef(cam.rotAngleX, 1.0f, 0.0f, 0.0f);
	glRotatef(cam.rotAngleY, 0.0f, 1.0f, 0.0f);
	glRotatef(cam.rotAngleZ, 1.0f, 0.0f, 0.0f);

	if (true)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_BLEND);
	}
}

void mt::GLRenderer2::renderBuffer()
{
	if (buffer.triangles)
	{
		// Render in wireframe atm
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_LIGHTING);
		}

		glColor4fv(glm::value_ptr(buffer.color));

		glBegin(GL_TRIANGLES);
		{
			for (int i = 0; i < buffer.numIndexes; i++)
			{
				auto &index = buffer.indexes[i];

				glNormal3fv(glm::value_ptr(buffer.normals[index.x]));

				glTexCoord2fv(glm::value_ptr(buffer.texCoords[index.x]));
				glVertex3fv(glm::value_ptr(buffer.xyz[index.x]));

				glTexCoord2fv(glm::value_ptr(buffer.texCoords[index.y]));
				glVertex3fv(glm::value_ptr(buffer.xyz[index.y]));

				glTexCoord2fv(glm::value_ptr(buffer.texCoords[index.z]));
				glVertex3fv(glm::value_ptr(buffer.xyz[index.z]));
			}
		}
		glEnd();

		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glEnable(GL_TEXTURE_2D);
			glDisable(GL_LIGHTING);
		}
	}
	else
	{
		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
		{
			glLineWidth(buffer.lineWidth);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_LIGHTING);
			glDisable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);

			glColor4fv(glm::value_ptr(buffer.color));

			glBegin(GL_LINES);
			{
				for (int i = 0; i < buffer.numVertices; i = i + 2)
				{
					glVertex3fv(glm::value_ptr(buffer.xyz[i]));
					glVertex3fv(glm::value_ptr(buffer.xyz[i + 1]));
				}
			}
			glEnd();

			glLineWidth(1);
		}
		glPopAttrib();
	}
}

void mt::GLRenderer2::endFrame()
{

}
