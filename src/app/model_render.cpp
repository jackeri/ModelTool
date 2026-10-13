#include "model_render.h"
#include "state.h"

namespace mt {

	model::PoseOptions poseOptions(Model &model, const ViewSettings &view, const model::Playback &playback)
	{
		model::PoseOptions options;
		options.bindPose = model.skeletalModel() && model.currentFrame == FRAME_BIND_POSE;
		options.frame = std::max(model.currentFrame, 0);
		options.nextFrame = options.frame;

		if (playback.isPlaying() && model.hasFrames() && playback.frame() == model.currentFrame)
		{
			options.nextFrame = std::clamp(playback.nextFrame(), 0, model.numFrames() - 1);
			options.blend = playback.blend();
		}

		options.lodLevel = view.lodLevel;
		options.dynamicLod = view.dynamicLod;
		options.lodRatio = view.lodRatio;
		return options;
	}

	glm::mat4 modelTransform(const Model &model)
	{
		return glm::translate(model.transform.location) * glm::toMat4(model.transform.rotation) * glm::scale(glm::vec3(model.scale));
	}

	bool posedBounds(const model::PosedModel &posed, Bounds &out)
	{
		bool any = false;
		auto add = [&](const glm::vec3 &pos) {
			out.min = any ? glm::min(out.min, pos) : pos;
			out.max = any ? glm::max(out.max, pos) : pos;
			any = true;
		};

		for (const auto &mesh : posed.meshes)
		{
			std::for_each(mesh.positions.begin(), mesh.positions.end(), add);
		}
		std::for_each(posed.joints.begin(), posed.joints.end(), add);
		return any;
	}

	void drawModel(Renderer &renderer, const model::PosedModel &posed, const ViewSettings &view, TextureCache &textures,
				   io::FileSource &source, const std::string &modelPath, float size)
	{
		for (const auto &mesh : posed.meshes)
		{
			MeshData data{mesh.positions, mesh.normals, mesh.texCoords, mesh.tris};

			MeshStyle style;
			style.wireframe = view.wireframe;
			style.alpha = view.alpha;
			if (view.wireframe)
			{
				style.color = {1, 1, 1, 1};
			}
			else if (view.textures)
			{
				style.texture = textures.get(source, mesh.material, modelPath);
			}
			if (!view.wireframe && !style.texture)
			{
				style.color = {0.8f, 0.8f, 0.8f, 1};
			}

			renderer.drawMesh(data, style);

			if (view.drawNormals)
			{
				std::vector<LineVertex> lines;
				for (size_t v = 0; v < mesh.positions.size() && v < mesh.normals.size(); v++)
				{
					lines.push_back({mesh.positions[v], {1, 1, 0, 1}});
					lines.push_back({mesh.positions[v] + mesh.normals[v] * view.normalLength * size, {1, 1, 0, 1}});
				}
				renderer.drawLines(lines, true);
			}
		}

		if (view.drawSkeleton)
		{
			std::vector<LineVertex> lines, points;
			for (const auto &[from, to] : posed.bones)
			{
				lines.push_back({from, {0, 1, 0, 1}});
				lines.push_back({to, {0, 1, 0, 1}});
			}
			for (const auto &joint : posed.joints)
			{
				points.push_back({joint, {1, 0, 0, 1}});
			}
			renderer.drawLines(lines, false);
			renderer.drawPoints(points, 5.0f, false);
		}

		if (view.drawTags)
		{
			for (const auto &tag : posed.tags)
			{
				renderer.push(glm::translate(tag.point.location) * glm::toMat4(tag.point.rotation));
				renderer.axis(size * 0.025f);
				renderer.pop();
			}
		}

		if (view.drawHitboxes)
		{
			for (size_t i = 0; i < posed.hitboxes.size(); i++)
			{
				const auto &hitbox = posed.hitboxes[i];
				const glm::vec4 color = int(i) == view.selectedHitbox ? glm::vec4{1, 1, 0, 1} : glm::vec4{1, 0, 1, 1};
				std::vector<LineVertex> lines;
				for (const auto &[from, to] : hitbox.hitbox.lines())
				{
					lines.push_back({from, color});
					lines.push_back({to, color});
				}
				renderer.push(glm::translate(hitbox.point.location) * glm::toMat4(hitbox.point.rotation));
				renderer.drawLines(lines, false);
				renderer.pop();
			}
		}

		if (view.drawBounds && posed.hasBounds)
		{
			renderer.bounds(posed.bounds, {1, 1, 1, 1});
		}
	}
}
