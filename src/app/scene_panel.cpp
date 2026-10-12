#include "scene_panel.h"
#include "framebuffer.h"
#include "camera.h"
#include "gl_renderer.h"
#include "model_render.h"
#include "state.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace mt {

	ScenePanel::ScenePanel()
	{
		renderer = make_ref<GLRenderer>();
		fbo = make_ref<Framebuffer>();
	}

	ScenePanel::~ScenePanel() = default;

	void ScenePanel::renderScene(int width, int height)
	{
		auto &state = singleton<State>();
		size = {std::max(width, 1), std::max(height, 1)};
		if (viewSize.x <= 0 || viewSize.y <= 0)
		{
			viewSize = size;
		}
		renderer->pixelScale = float(size.y) / viewSize.y;

		state.textures.setOptions(state.view.picmip, state.view.textureFilter);

		posed = {};
		modelMatrix = glm::mat4{1.0f};
		posedAttachments.clear();
		attachmentMatrices.clear();
		if (state.model)
		{
			posed = model::poseModel(*state.model, poseOptions(*state.model, state.view, state.playback));
			model::applySkin(posed, state.skin);
			modelMatrix = modelTransform(*state.model);

			// Attached models follow the tag of their parent (parents come first in the list)
			for (auto &attachment : state.attachments)
			{
				const bool onMain = attachment.parent < 0 || attachment.parent >= int(posedAttachments.size());
				const model::PosedModel &parent = onMain ? posed : posedAttachments[size_t(attachment.parent)];
				const glm::mat4 &parentMatrix = onMain ? modelMatrix : attachmentMatrices[size_t(attachment.parent)];

				glm::mat4 tag{1.0f};
				model::tagMatrix(parent, attachment.tag, tag);
				attachmentMatrices.push_back(parentMatrix * tag * modelTransform(*attachment.model));

				posedAttachments.push_back(
					model::poseModel(*attachment.model, poseOptions(*attachment.model, state.view, attachment.playback)));
				model::applySkin(posedAttachments.back(), attachment.skin);
			}
		}

		if (state.resetCamera)
		{
			camera.modelViewCamera();
			state.resetCamera = false;
			state.frameCamera = !!state.model;
		}

		// Bounds of the model in the scene, every corner is transformed as the model can be rotated
		Bounds sceneBounds{glm::vec3(-60), glm::vec3(60)};
		bool anyBounds = false;
		auto addBounds = [&](const model::PosedModel &posedModel, const glm::mat4 &matrix) {
			Bounds local{};
			if (!posedBounds(posedModel, local))
			{
				return;
			}
			for (int corner = 0; corner < 8; corner++)
			{
				glm::vec3 point{corner & 1 ? local.max.x : local.min.x, corner & 2 ? local.max.y : local.min.y,
								corner & 4 ? local.max.z : local.min.z};
				point = glm::vec3(matrix * glm::vec4(point, 1.0f));
				sceneBounds.min = anyBounds ? glm::min(sceneBounds.min, point) : point;
				sceneBounds.max = anyBounds ? glm::max(sceneBounds.max, point) : point;
				anyBounds = true;
			}
		};
		addBounds(posed, modelMatrix);
		for (size_t i = 0; i < posedAttachments.size(); i++)
		{
			addBounds(posedAttachments[i], attachmentMatrices[i]);
		}
		Bounds local{};
		posedBounds(posed, local);
		const float modelSize = std::max(glm::length(local.max - local.min), 1.0f);

		if (state.frameCamera)
		{
			camera.frame(sceneBounds);
			state.frameCamera = false;
		}

		fbo->create(size.x, size.y);
		fbo->bind();

		const auto &color = state.clearColor.Value;
		renderer->startFrame(camera, size.x, size.y, {color.x, color.y, color.z, color.w});

		if (state.view.drawGrid)
		{
			renderer->grid(std::max(std::max(std::abs(sceneBounds.min.x), std::abs(sceneBounds.max.x)),
									std::max(std::abs(sceneBounds.min.y), std::abs(sceneBounds.max.y)))
							   * 2.5f,
						   Renderer::gridColor({color.x, color.y, color.z, 1}));
		}

		if (state.model)
		{
			renderer->push(modelMatrix);
			drawModel(*renderer, posed, state.view, state.textures, state.filesystem, state.modelPath, modelSize);
			renderer->pop();

			for (size_t i = 0; i < posedAttachments.size(); i++)
			{
				renderer->push(attachmentMatrices[i]);
				drawModel(*renderer, posedAttachments[i], state.view, state.textures, state.filesystem, state.attachments[i].path,
						  modelSize);
				renderer->pop();
			}
		}

		renderer->endFrame();
		fbo->unbind();
	}

	bool ScenePanel::saveImage(const std::string &path)
	{
		std::vector<byte> pixels(size_t(size.x) * size_t(size.y) * 4);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, fbo->texture());
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);

		return writePng(path, size.x, size.y, pixels);
	}

	bool ScenePanel::writePng(const std::string &path, int width, int height, const std::vector<byte> &pixels)
	{
		// OpenGL rows go from the bottom up
		stbi_flip_vertically_on_write(1);
		return stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
	}

	void ScenePanel::render()
	{
		ImGuiWindowFlags sceneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
		ImGui::Begin("Scene", nullptr, sceneFlags);

		// The framebuffer has the screen's pixels, on HiDPI screens more than the window size
		ImVec2 available = ImGui::GetContentRegionAvail();
		const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
		viewSize = {std::max(available.x, 1.0f), std::max(available.y, 1.0f)};
		renderScene(to_int(viewSize.x * scale.x), to_int(viewSize.y * scale.y));

		ImVec2 origin = ImGui::GetCursorScreenPos();
		auto textureId = reinterpret_cast<ImTextureID>(intptr_t(fbo->texture()));
		ImGui::Image(textureId, ImVec2{viewSize.x, viewSize.y}, ImVec2{0, 1}, ImVec2{1, 0});
		sceneHovered = ImGui::IsItemHovered();

		drawLabels(origin);
		handleInput();

		ImGui::End();
	}

	void ScenePanel::handleInput()
	{
		auto &io = ImGui::GetIO();

		if (sceneHovered && io.MouseWheel != 0)
		{
			camera.zoom(io.MouseWheel);
		}

		// Dragging started in the scene keeps working outside of it
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && (sceneHovered || ImGui::IsItemActive()))
		{
			camera.rotate(io.MouseDelta.x, io.MouseDelta.y);
		}
		else if ((ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) && sceneHovered)
		{
			camera.pan(io.MouseDelta.x, io.MouseDelta.y, viewSize.y);
		}
	}

	void ScenePanel::drawLabels(const ImVec2 &origin)
	{
		auto &view = singleton<State>().view;
		if (!view.drawTags || !view.drawTagNames || posed.tags.empty())
		{
			return;
		}

		const glm::mat4 matrix = renderer->viewProjection() * modelMatrix;
		ImDrawList *drawList = ImGui::GetWindowDrawList();

		for (const auto &tag : posed.tags)
		{
			glm::vec4 clip = matrix * glm::vec4(tag.point.location, 1.0f);
			if (clip.w <= 0.0f)
			{
				continue;
			}

			glm::vec3 ndc = glm::vec3(clip) / clip.w;
			ImVec2 screen{origin.x + (ndc.x * 0.5f + 0.5f) * viewSize.x + 6.0f, origin.y + (0.5f - ndc.y * 0.5f) * viewSize.y - 6.0f};
			drawList->AddText(screen, IM_COL32(255, 0, 255, 255), tag.name.c_str());
		}
	}
}
