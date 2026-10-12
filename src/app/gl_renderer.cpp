#include "gl_renderer.h"
#include "camera.h"
#include "texture.h"

namespace mt {

	namespace {
		const char *MESH_VERTEX_SHADER = R"(#version 410 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMvp;
uniform mat3 uNormalMatrix;

out vec3 vNormal;
out vec2 vTexCoord;

void main()
{
	vNormal = uNormalMatrix * aNormal;
	vTexCoord = aTexCoord;
	gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

		const char *MESH_FRAGMENT_SHADER = R"(#version 410 core
in vec3 vNormal;
in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform bool uLighting;
uniform bool uAlpha;
uniform vec4 uColor;

out vec4 fragColor;

void main()
{
	vec4 color = uColor;
	if (uUseTexture)
	{
		color *= texture(uTexture, vTexCoord);
	}

	if (uLighting)
	{
		// Two fixed lights in world space, like the old fixed function setup
		vec3 normal = length(vNormal) > 0.0 ? normalize(vNormal) : vec3(0.0, 0.0, 1.0);
		float key = max(dot(normal, normalize(vec3(0.55, -0.5, 0.65))), 0.0);
		float fill = max(dot(normal, normalize(vec3(-0.5, 0.45, 0.15))), 0.0);
		color.rgb *= min(0.35 + 0.75 * key + 0.3 * fill, 1.25);
	}

	if (!uAlpha)
	{
		color.a = 1.0;
	}
	fragColor = color;
}
)";

		const char *LINE_VERTEX_SHADER = R"(#version 410 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec4 aColor;

uniform mat4 uMvp;
uniform float uPointSize;

out vec4 vColor;

void main()
{
	vColor = aColor;
	gl_PointSize = uPointSize;
	gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

		const char *LINE_FRAGMENT_SHADER = R"(#version 410 core
in vec4 vColor;
out vec4 fragColor;

void main()
{
	fragColor = vColor;
}
)";
	}

	GLRenderer::GLRenderer()
	{
		meshShader.init(MESH_VERTEX_SHADER, MESH_FRAGMENT_SHADER);
		lineShader.init(LINE_VERTEX_SHADER, LINE_FRAGMENT_SHADER);

		if (!meshShader.valid() || !lineShader.valid())
		{
			throw mt_ex("Failed to build the shaders");
		}

		glGenVertexArrays(1, &meshVao);
		glGenBuffers(1, &meshVbo);
		glGenBuffers(1, &meshIbo);
		glBindVertexArray(meshVao);
		glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIbo);
		const GLsizei meshStride = 8 * sizeof(float);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, meshStride, reinterpret_cast<void *>(0));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, meshStride, reinterpret_cast<void *>(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, meshStride, reinterpret_cast<void *>(6 * sizeof(float)));

		glGenVertexArrays(1, &lineVao);
		glGenBuffers(1, &lineVbo);
		glBindVertexArray(lineVao);
		glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), reinterpret_cast<void *>(offsetof(LineVertex, pos)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), reinterpret_cast<void *>(offsetof(LineVertex, color)));

		glBindVertexArray(0);

		const byte white[4] = {255, 255, 255, 255};
		glGenTextures(1, &whiteTexture);
		glBindTexture(GL_TEXTURE_2D, whiteTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	GLRenderer::~GLRenderer()
	{
		glDeleteTextures(1, &whiteTexture);
		glDeleteBuffers(1, &meshVbo);
		glDeleteBuffers(1, &meshIbo);
		glDeleteBuffers(1, &lineVbo);
		glDeleteVertexArrays(1, &meshVao);
		glDeleteVertexArrays(1, &lineVao);
	}

	void GLRenderer::startFrame(const Camera &camera, int width, int height, const glm::vec4 &clearColor)
	{
		glViewport(0, 0, width, height);
		glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDisable(GL_CULL_FACE);
		glEnable(GL_PROGRAM_POINT_SIZE);

		viewProj = camera.projection(height > 0 ? float(width) / float(height) : 1.0f) * camera.view();
		stack.assign(1, glm::mat4{1.0f});
	}

	void GLRenderer::endFrame()
	{
		glBindVertexArray(0);
		glUseProgram(0);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_BLEND);
	}

	void GLRenderer::drawMesh(const MeshData &mesh, const MeshStyle &style)
	{
		if (mesh.tris.empty() || mesh.positions.empty())
		{
			return;
		}

		meshVertices.clear();
		meshVertices.reserve(mesh.positions.size() * 8);
		for (size_t i = 0; i < mesh.positions.size(); i++)
		{
			const glm::vec3 &pos = mesh.positions[i];
			const glm::vec3 normal = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3{0};
			const glm::vec2 tex = i < mesh.texCoords.size() ? mesh.texCoords[i] : glm::vec2{0};
			meshVertices.insert(meshVertices.end(), {pos.x, pos.y, pos.z, normal.x, normal.y, normal.z, tex.x, tex.y});
		}

		meshIndices.clear();
		meshIndices.reserve(mesh.tris.size() * 3);
		const auto numVertices = uint32_t(mesh.positions.size());
		for (const auto &tri : mesh.tris)
		{
			// Skip triangles with invalid indices instead of reading outside the buffer
			if (uint32_t(tri.x) >= numVertices || uint32_t(tri.y) >= numVertices || uint32_t(tri.z) >= numVertices)
			{
				continue;
			}
			meshIndices.insert(meshIndices.end(), {uint32_t(tri.x), uint32_t(tri.y), uint32_t(tri.z)});
		}

		glBindVertexArray(meshVao);
		glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
		glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(meshVertices.size() * sizeof(float)), meshVertices.data(), GL_STREAM_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIbo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(meshIndices.size() * sizeof(uint32_t)), meshIndices.data(), GL_STREAM_DRAW);

		meshShader.use();
		meshShader.setUniform("uMvp", viewProj * modelMatrix());
		meshShader.setUniform("uNormalMatrix", glm::transpose(glm::inverse(glm::mat3(modelMatrix()))));
		meshShader.setUniform("uColor", style.color);
		meshShader.setUniform("uLighting", style.lighting && !style.wireframe);
		meshShader.setUniform("uAlpha", style.alpha);
		meshShader.setUniform("uUseTexture", style.texture != nullptr && !style.wireframe);
		meshShader.setUniform("uTexture", 0);

		glActiveTexture(GL_TEXTURE0);
		if (style.texture && !style.wireframe)
		{
			style.texture->bind();
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, whiteTexture);
		}

		if (style.alpha || style.color.a < 1.0f)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
		{
			glDisable(GL_BLEND);
		}

		if (style.wireframe)
		{
			// Draw the wire slightly in front of a filled pass of the same mesh
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glEnable(GL_POLYGON_OFFSET_LINE);
			glPolygonOffset(-1.0f, -1.0f);
		}

		glDrawElements(GL_TRIANGLES, GLsizei(meshIndices.size()), GL_UNSIGNED_INT, nullptr);

		if (style.wireframe)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glDisable(GL_POLYGON_OFFSET_LINE);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);
	}

	void GLRenderer::drawLineVertices(const std::vector<LineVertex> &vertices, GLenum mode, float pointSize, bool depthTest)
	{
		if (vertices.empty())
		{
			return;
		}

		glBindVertexArray(lineVao);
		glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
		glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(LineVertex)), vertices.data(), GL_STREAM_DRAW);

		lineShader.use();
		lineShader.setUniform("uMvp", viewProj * modelMatrix());
		lineShader.setUniform("uPointSize", pointSize * pixelScale);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if (!depthTest)
		{
			glDisable(GL_DEPTH_TEST);
		}

		glDrawArrays(mode, 0, GLsizei(vertices.size()));

		glEnable(GL_DEPTH_TEST);
		glBindVertexArray(0);
	}

	void GLRenderer::drawLines(const std::vector<LineVertex> &vertices, bool depthTest)
	{
		drawLineVertices(vertices, GL_LINES, 1.0f, depthTest);
	}

	void GLRenderer::drawPoints(const std::vector<LineVertex> &vertices, float size, bool depthTest)
	{
		drawLineVertices(vertices, GL_POINTS, size, depthTest);
	}
}
