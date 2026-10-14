#include "scene_panel.h"
#include "framebuffer.h"
#include "camera.h"
#include "gl_renderer.h"
#include "model_render.h"
#include "state.h"
#include "point_edit.h"
#include "skeletal_model.h"
#include "morph_model.h"
#include "icons_font_awesome.h"
#include <ImGuizmo.h>

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

		// The grid and the overlays (tag axes, normals) follow the size of the whole animation, not the current pose, so
		// they do not change while the model moves
		const Model *current = state.model.get();
		const int frames = current ? current->numFrames() : 0;
		if (current != stableModel || frames != stableFrames || modelMatrix != stableTransform)
		{
			stableModel = current;
			stableFrames = frames;
			stableTransform = modelMatrix;
			updateStableSizes(sceneBounds, local);
		}
		const float modelSize = overlaySize;

		if (state.frameCamera)
		{
			camera.frame(sceneBounds);
			state.frameCamera = false;
		}

		fbo->create(size.x, size.y);
		fbo->bind();

		const auto &color = state.clearColor.Value;
		renderer->startFrame(camera, size.x, size.y, {color.x, color.y, color.z, color.w});

		if (state.background == Background::Gradient)
		{
			const auto &bottom = state.backgroundBottom.Value;
			renderer->background({color.x, color.y, color.z, 1}, {bottom.x, bottom.y, bottom.z, 1});
		}
		else if (state.background == Background::SkyBox && !state.skyBox.empty())
		{
			// The game's side names, in its order
			static const char *suffixes[6] = {"_rt", "_bk", "_lf", "_ft", "_up", "_dn"};
			std::array<const Texture *, 6> sides{};
			for (size_t side = 0; side < 6; side++)
			{
				if (Texture *texture = state.textures.get(state.filesystem, state.skyBox + suffixes[side], ""))
				{
					texture->setClamp(true);
					sides[side] = texture;
				}
			}
			renderer->skyBox(sides, state.view.gamma, state.view.intensity);
		}

		if (state.view.drawGrid)
		{
			renderer->grid(gridSize, Renderer::gridColor({color.x, color.y, color.z, 1}));
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

		// Width, depth and height of everything shown: the model and its attachments, the meshes only
		measurements.clear();
		if (state.model && state.view.drawMeasurements)
		{
			Bounds box{};
			bool any = false;
			auto add = [&](const model::PosedModel &posedModel, const glm::mat4 &matrix) {
				for (const auto &mesh : posedModel.meshes)
				{
					for (const auto &position : mesh.positions)
					{
						const glm::vec3 point = glm::vec3(matrix * glm::vec4(position, 1.0f));
						box.min = any ? glm::min(box.min, point) : point;
						box.max = any ? glm::max(box.max, point) : point;
						any = true;
					}
				}
			};
			add(posed, modelMatrix);
			for (size_t i = 0; i < posedAttachments.size(); i++)
			{
				add(posedAttachments[i], attachmentMatrices[i]);
			}

			if (any)
			{
				const glm::vec3 size = box.max - box.min;
				const float offset = std::max(std::max(size.x, std::max(size.y, size.z)) * 0.04f, 0.25f);
				const glm::vec4 colors[3] = {{1.0f, 0.45f, 0.45f, 1}, {0.45f, 1.0f, 0.45f, 1}, {0.5f, 0.6f, 1.0f, 1}};
				std::vector<LineVertex> lines;
				for (const auto &line : model::dimensionLines(box, offset))
				{
					const glm::vec4 &lineColor = colors[line.axis];
					lines.push_back({line.from, lineColor});
					lines.push_back({line.to, lineColor});
					for (const auto &[from, to] : line.extensions)
					{
						lines.push_back({from, lineColor * glm::vec4(1, 1, 1, 0.6f)});
						lines.push_back({to, lineColor * glm::vec4(1, 1, 1, 0.6f)});
					}
					measurements.push_back(line);
				}
				renderer->drawLines(lines, false);
			}
		}

		renderer->endFrame();
		fbo->unbind();
	}

	void ScenePanel::updateStableSizes(const Bounds &poseBounds, const Bounds &poseLocal)
	{
		auto &state = singleton<State>();

		// Every frame's bounds as stored in the model (the bounds the game uses), else the current pose
		std::vector<Bounds> frames;
		if (const auto *skeletal = dynamic_cast<const model::skeletal::SkeletalModel *>(state.model.get()))
		{
			frames = skeletal->bounds;
		}
		else if (const auto *morph = dynamic_cast<const model::morph::MorphModel *>(state.model.get()))
		{
			for (const auto &frame : morph->frames)
			{
				frames.push_back(frame.bounds);
			}
		}

		float extent = 0;
		auto add = [&](const glm::vec3 &point) { extent = std::max({extent, std::abs(point.x), std::abs(point.y)}); };
		Bounds all{};
		for (size_t i = 0; i < frames.size(); i++)
		{
			const Bounds &bounds = frames[i];
			all.min = i ? glm::min(all.min, bounds.min) : bounds.min;
			all.max = i ? glm::max(all.max, bounds.max) : bounds.max;
			for (int corner = 0; corner < 8; corner++)
			{
				const glm::vec3 point{corner & 1 ? bounds.max.x : bounds.min.x, corner & 2 ? bounds.max.y : bounds.min.y,
									  corner & 4 ? bounds.max.z : bounds.min.z};
				add(glm::vec3(modelMatrix * glm::vec4(point, 1.0f)));
			}
		}
		if (extent <= 0)
		{
			add(poseBounds.min);
			add(poseBounds.max);
			all = poseLocal;
		}
		gridSize = extent * 2.5f;
		overlaySize = std::max(glm::length(all.max - all.min), 1.0f);
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
		const bool imageHovered = ImGui::IsItemHovered();

		drawLabels(origin);
		drawEditGizmo(origin);
		if (singleton<State>().view.drawGizmo)
		{
			drawGizmo(origin);
		}

		// The toolbar buttons and the edit gizmo take the mouse before the camera
		sceneHovered = imageHovered && !ImGui::IsAnyItemHovered();
		handleInput();

		ImGui::End();
	}

	void ScenePanel::handleInput()
	{
		auto &io = ImGui::GetIO();

		// A drag that starts on the edit gizmo moves the tag or hitbox, not the camera
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			gizmoDrag = ImGuizmo::IsOver() || ImGui::IsAnyItemActive();
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			gizmoDrag = false;
		}
		if (gizmoDrag || ImGuizmo::IsUsing())
		{
			return;
		}

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
		drawMeasurementLabels(origin);
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

	void ScenePanel::drawMeasurementLabels(const ImVec2 &origin)
	{
		if (measurements.empty())
		{
			return;
		}

		const auto unit = singleton<State>().view.lengthUnit;
		const glm::mat4 &matrix = renderer->viewProjection();
		ImDrawList *drawList = ImGui::GetWindowDrawList();
		const ImU32 colors[3] = {IM_COL32(255, 120, 120, 255), IM_COL32(120, 255, 120, 255), IM_COL32(140, 160, 255, 255)};

		for (const auto &line : measurements)
		{
			const glm::vec4 clip = matrix * glm::vec4(line.label, 1.0f);
			if (clip.w <= 0.0f)
			{
				continue;
			}
			const glm::vec3 ndc = glm::vec3(clip) / clip.w;
			const ImVec2 at{origin.x + (ndc.x * 0.5f + 0.5f) * viewSize.x, origin.y + (0.5f - ndc.y * 0.5f) * viewSize.y};

			// The value on a dark box next to the middle of the line, readable on any background
			const std::string text = std::string(model::dimensionName(line.axis)) + " " + model::formatLength(line.length, unit);
			const ImVec2 size = ImGui::CalcTextSize(text.c_str());
			const ImVec2 corner{at.x + 6.0f, at.y - size.y * 0.5f};
			drawList->AddRectFilled({corner.x - 3.0f, corner.y - 1.0f}, {corner.x + size.x + 3.0f, corner.y + size.y + 1.0f},
									IM_COL32(0, 0, 0, 170), 3.0f);
			drawList->AddText(corner, colors[line.axis], text.c_str());
		}
	}

	void ScenePanel::drawGizmo(const ImVec2 &origin)
	{
		struct Axis {
			int axis;
			bool negative;
			const char *label;
			const char *direction; ///< idTech direction of the axis
			const char *side;	   ///< what the view from this side shows
			ImU32 color;
		};
		static const Axis axes[6] = {
			{0, false, "X", "+X forward", "front", IM_COL32(230, 70, 70, 255)},
			{0, true, "", "-X back", "back", IM_COL32(230, 70, 70, 255)},
			{1, false, "Y", "+Y left", "left", IM_COL32(90, 200, 90, 255)},
			{1, true, "", "-Y right", "right", IM_COL32(90, 200, 90, 255)},
			{2, false, "Z", "+Z up", "top", IM_COL32(80, 130, 240, 255)},
			{2, true, "", "-Z down", "bottom", IM_COL32(80, 130, 240, 255)},
		};

		const float fontSize = ImGui::GetFontSize();
		const float radius = fontSize * 2.8f;
		const float knob = fontSize * 0.62f;
		const float margin = fontSize * 0.8f;
		const ImVec2 center{origin.x + viewSize.x - margin - radius - knob, origin.y + margin + radius + knob};
		if (viewSize.x < (radius + knob + margin) * 3 || viewSize.y < (radius + knob + margin) * 3)
		{
			return;
		}

		// Axis ends on the screen, the farthest drawn first
		const glm::mat3 rotation = camera.rotation();
		struct Placed {
			const Axis *axis;
			ImVec2 position;
			float depth;
		};
		std::vector<Placed> placed;
		for (const auto &axis : axes)
		{
			glm::vec3 direction{0.0f};
			direction[axis.axis] = axis.negative ? -1.0f : 1.0f;
			const glm::vec3 view = rotation * direction;
			placed.push_back({&axis, {center.x + view.x * radius, center.y - view.y * radius}, view.z});
		}
		std::sort(placed.begin(), placed.end(), [](const Placed &a, const Placed &b) { return a.depth < b.depth; });

		const ImVec2 mouse = ImGui::GetMousePos();
		const Placed *hovered = nullptr;
		if (ImGui::IsWindowHovered())
		{
			// The nearest knob under the mouse
			for (const auto &item : placed)
			{
				const float dx = mouse.x - item.position.x, dy = mouse.y - item.position.y;
				if (dx * dx + dy * dy <= knob * knob)
				{
					hovered = &item;
				}
			}
		}

		ImDrawList *drawList = ImGui::GetWindowDrawList();
		const bool overGizmo = ImGui::IsWindowHovered()
							   && (mouse.x - center.x) * (mouse.x - center.x) + (mouse.y - center.y) * (mouse.y - center.y)
									  <= (radius + knob) * (radius + knob);
		drawList->AddCircleFilled(center, radius + knob, IM_COL32(255, 255, 255, overGizmo ? 28 : 12));

		auto withAlpha = [](ImU32 color, int alpha) { return (color & ~IM_COL32_A_MASK) | (ImU32(alpha) << IM_COL32_A_SHIFT); };
		for (const auto &item : placed)
		{
			const Axis &axis = *item.axis;
			const bool isHovered = hovered == &item;
			if (axis.negative)
			{
				drawList->AddCircleFilled(item.position, knob * 0.8f, withAlpha(axis.color, isHovered ? 200 : 70));
				drawList->AddCircle(item.position, knob * 0.8f, withAlpha(axis.color, 220), 0, 1.5f);
				continue;
			}

			drawList->AddLine(center, item.position, axis.color, 2.0f);
			drawList->AddCircleFilled(item.position, knob, isHovered ? IM_COL32(255, 255, 255, 255) : axis.color);
			const ImVec2 text = ImGui::CalcTextSize(axis.label);
			drawList->AddText({item.position.x - text.x * 0.5f, item.position.y - text.y * 0.5f}, IM_COL32(20, 20, 20, 255), axis.label);
		}

		// Legend under the gizmo, one colored line per axis, aligned to the right edge of the scene
		float legendY = center.y + radius + knob + fontSize * 0.3f;
		for (const auto &axis : axes)
		{
			if (axis.negative)
			{
				continue;
			}
			const ImVec2 size = ImGui::CalcTextSize(axis.direction);
			drawList->AddText({origin.x + viewSize.x - margin - size.x, legendY}, withAlpha(axis.color, 210), axis.direction);
			legendY += fontSize * 1.1f;
		}

		if (hovered)
		{
			ImGui::SetTooltip("%s\nClick to look from the %s", hovered->axis->direction, hovered->axis->side);
			// A click, not the end of a drag that orbits the camera
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 9.0f)
			{
				camera.viewFrom(hovered->axis->axis, hovered->axis->negative);
			}
		}
	}

	void ScenePanel::drawEditGizmo(const ImVec2 &origin)
	{
		auto &state = singleton<State>();
		auto &view = state.view;
		if (!state.model || view.editTarget == EditTarget::None)
		{
			return;
		}

		using model::pointMatrix;
		auto *skeletal = dynamic_cast<model::skeletal::SkeletalModel *>(state.model.get());
		auto *morph = dynamic_cast<model::morph::MorphModel *>(state.model.get());

		// Bones of the current pose (posed every frame by renderScene)
		auto boneMatrix = [&](int bone) {
			return bone >= 0 && bone < int(posed.bonePoints.size()) ? pointMatrix(posed.bonePoints[size_t(bone)].point) : glm::mat4{1.0f};
		};

		// The edited point: its parent's world matrix and its own
		glm::mat4 parent = modelMatrix;
		Point *point = nullptr;
		model::Hitbox *hitbox = nullptr;
		std::string label;
		int morphFrame = 0;
		if (view.editTarget == EditTarget::Hitbox && skeletal && view.selectedHitbox >= 0
			&& view.selectedHitbox < int(skeletal->hitboxes.size()))
		{
			hitbox = &skeletal->hitboxes[size_t(view.selectedHitbox)];
			parent = modelMatrix * boneMatrix(hitbox->parentJoint);
			point = hitbox;
			label = "Hitbox " + hitbox->name;
		}
		else if (view.editTarget == EditTarget::Tag && skeletal && view.selectedTag >= 0 && view.selectedTag < int(skeletal->tags.size()))
		{
			auto &tag = skeletal->tags[size_t(view.selectedTag)];
			parent = modelMatrix * boneMatrix(tag.parentBone);
			point = &tag;
			label = "Tag " + tag.name;
		}
		else if (view.editTarget == EditTarget::Tag && morph && view.selectedTag >= 0 && view.selectedTag < int(morph->tags.size())
				 && !morph->tags[size_t(view.selectedTag)].frames.empty())
		{
			auto &tag = morph->tags[size_t(view.selectedTag)];
			morphFrame = std::clamp(state.model->currentFrame, 0, int(tag.frames.size()) - 1);
			point = &tag.frames[size_t(morphFrame)];
			label = "Tag " + tag.name;
		}
		if (!point)
		{
			return;
		}

		drawEditToolbar(origin, label, morph != nullptr, hitbox != nullptr);
		const GizmoMode mode = view.gizmoMode == GizmoMode::Scale && !hitbox ? GizmoMode::Move : view.gizmoMode;

		glm::mat4 world = parent * pointMatrix(*point);
		const glm::mat4 viewMatrix = camera.view();
		const glm::mat4 projection = camera.projection(viewSize.x / std::max(viewSize.y, 1.0f));

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(origin.x, origin.y, viewSize.x, viewSize.y);

		// The scale gizmo gives the scale since the drag started, the sizes scale from the hitbox at that moment
		if (hitbox && !ImGuizmo::IsUsing())
		{
			scaleStart = *hitbox;
		}

		// Ctrl snaps: 1 unit steps, 15 degree steps, 10% steps
		const float snapValue = mode == GizmoMode::Rotate ? 15.0f : (mode == GizmoMode::Scale ? 0.1f : 1.0f);
		const float snap[3] = {snapValue, snapValue, snapValue};
		const bool snapping = ImGui::GetIO().KeyCtrl;
		const ImGuizmo::OPERATION operation
			= mode == GizmoMode::Rotate ? ImGuizmo::ROTATE : (mode == GizmoMode::Scale ? ImGuizmo::SCALE : ImGuizmo::TRANSLATE);
		// Scaling is always along the hitbox's own axes
		const ImGuizmo::MODE space = view.gizmoLocal || mode == GizmoMode::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
		if (ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projection), operation, space, glm::value_ptr(world), nullptr,
								 snapping ? snap : nullptr))
		{
			if (mode == GizmoMode::Scale)
			{
				// Scale on the hitbox's own axes, without the parent's (model) scale
				const glm::mat4 local = glm::inverse(parent) * world;
				const glm::vec3 scale{glm::length(glm::vec3(local[0])), glm::length(glm::vec3(local[1])), glm::length(glm::vec3(local[2]))};
				hitbox->scaleFrom(scaleStart, scale);
				return;
			}

			const Point local = model::localPoint(parent, world);
			if (morph && view.editTarget == EditTarget::Tag)
			{
				model::moveMorphTag(*morph, view.selectedTag, morphFrame, local, view.gizmoAllFrames);
			}
			else
			{
				point->location = local.location;
				point->rotation = local.rotation;
			}
		}
	}

	void ScenePanel::drawEditToolbar(const ImVec2 &origin, const std::string &label, bool morphTag, bool canScale)
	{
		auto &view = singleton<State>().view;
		const ImVec2 padding = ImGui::GetStyle().WindowPadding;
		ImGui::SetCursorScreenPos({origin.x + padding.x, origin.y + padding.y});

		ImGui::BeginGroup();
		ImGui::TextUnformatted(label.c_str());

		auto toggle = [](const char *text, bool active, const char *tooltip) {
			if (active)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}
			const bool clicked = ImGui::Button(text);
			if (active)
			{
				ImGui::PopStyleColor();
			}
			ImGui::SetItemTooltip("%s", tooltip);
			return clicked;
		};

		const bool scaling = view.gizmoMode == GizmoMode::Scale && canScale;
		if (toggle(ICON_FA_ARROWS " Move", view.gizmoMode == GizmoMode::Move || (view.gizmoMode == GizmoMode::Scale && !canScale),
				   "Move (G), hold Ctrl to snap to 1 unit steps"))
		{
			view.gizmoMode = GizmoMode::Move;
		}
		ImGui::SameLine();
		if (toggle(ICON_FA_REFRESH " Rotate", view.gizmoMode == GizmoMode::Rotate, "Rotate (R), hold Ctrl to snap to 15 degree steps"))
		{
			view.gizmoMode = GizmoMode::Rotate;
		}
		ImGui::SameLine();
		if (canScale)
		{
			if (toggle(ICON_FA_EXPAND " Scale", scaling, "Scale the size (S), hold Ctrl to snap to 10% steps"))
			{
				view.gizmoMode = GizmoMode::Scale;
			}
			ImGui::SameLine();
		}
		if (toggle(view.gizmoLocal ? "Local" : "World", view.gizmoLocal, "Gizmo axes: the object's own (local) or the world's"))
		{
			view.gizmoLocal = !view.gizmoLocal;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TIMES))
		{
			view.editTarget = EditTarget::None;
			view.selectedTag = -1;
			view.selectedHitbox = -1;
		}
		ImGui::SetItemTooltip("Deselect and hide the gizmo (Escape)");

		if (morphTag)
		{
			ImGui::Checkbox("All frames", &view.gizmoAllFrames);
			ImGui::SetItemTooltip("Move the tag in every frame (keeps its animation), or only in the current frame");
		}
		ImGui::EndGroup();
	}
}
