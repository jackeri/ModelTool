/*
 * Fits a small hitbox set to ET characters (.char): the body mesh and the hud head, without the accessories. With
 * several characters (same bones, like every class of both teams) one hitbox set is fitted to all of them.
 *
 * Every body vertex goes to the hitbox of its strongest bone (head, torso, upper arms, forearms with the hands,
 * thighs, calves, feet). The head model is placed on the body's tag_head; the part of it below the head bone (the neck,
 * behind the bone along its X axis) goes to the torso, and the head is a box with its bottom on that plane, so the head
 * hitbox ends where the head starts. Each group is fitted in its bone's space to every 5th frame of
 * the animation group, so the set holds the mesh in every pose. The arm and leg parts are capsules (their round ends
 * meet without gaps at the elbows and knees); for the others a box, sphere, cylinder and capsule are fitted (the
 * rotation is searched for the smallest volume) and the smallest one is kept.
 *
 * How closely they fit: --coverage leaves out the vertex positions farthest out of each part (the shape is fitted, the
 * farthest points dropped, and it is fitted again to the rest), --margin is the space left around the held vertices.
 *
 * Build: cmake --build build --target fit_hitboxes
 * Run:   ./build/fit_hitboxes <base folder> <character.char>... <output.hitboxes> [--single-pose [--sequence name | --frame n]]
 *        --single-pose fits one pose only (alert_idle_2h by default): tighter, but parts of the mesh leave the
 *        hitboxes in other poses
 *        --coverage percent (default 100) and --margin units (default 0.05, negative fits inside the mesh)
 */
#include "et_character.h"
#include "filesystem.h"
#include "hitbox.h"
#include "posed_model.h"
#include "skeletal_model.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace mt;
using namespace mt::model;

namespace {
	constexpr float MAX_FLOAT = std::numeric_limits<float>::max();

	/*! Bones whose vertices share one hitbox, fitted in the space of the parent bone */
	struct Group {
		std::string name;
		int parent = -1;
		std::vector<int> joints;
		bool head = false;
		bool capsule = false; ///< always a capsule, else the smallest shape
	};

	float volume(const Hitbox &hitbox)
	{
		const float pi = glm::pi<float>();
		const float r = hitbox.radius;
		switch (hitbox.shape)
		{
		case HitboxShape::Sphere: return 4.0f / 3.0f * pi * r * r * r;
		case HitboxShape::Cylinder: return pi * r * r * hitbox.height;
		case HitboxShape::Capsule: return pi * r * r * (hitbox.height - 2 * r) + 4.0f / 3.0f * pi * r * r * r;
		case HitboxShape::Box:
		default: {
			const glm::vec3 size = hitbox.max - hitbox.min;
			return size.x * size.y * size.z;
		}
		}
	}

	/*! Is a point (in the hitbox's parent space) inside the hitbox */
	bool inside(const Hitbox &hitbox, const glm::vec3 &point, float slack = 0.0f)
	{
		const glm::vec3 local = glm::inverse(hitbox.rotation) * (point - hitbox.location);
		switch (hitbox.shape)
		{
		case HitboxShape::Sphere: return glm::length(local) <= hitbox.radius + slack;
		case HitboxShape::Cylinder:
			return glm::length(glm::vec2(local)) <= hitbox.radius + slack && std::abs(local.z) <= hitbox.height / 2 + slack;
		case HitboxShape::Capsule: {
			const float half = std::max(hitbox.height / 2 - hitbox.radius, 0.0f);
			return glm::distance(local, glm::vec3(0, 0, std::clamp(local.z, -half, half))) <= hitbox.radius + slack;
		}
		case HitboxShape::Box:
		default:
			return glm::all(glm::greaterThanEqual(local, hitbox.min - slack)) && glm::all(glm::lessThanEqual(local, hitbox.max + slack));
		}
	}

	using Circle = std::pair<glm::vec2, float>;

	/*! Smallest circle around 2D points (Welzl) */
	Circle minCircle(std::vector<glm::vec2> points)
	{
		std::shuffle(points.begin(), points.end(), std::mt19937(1));

		auto fromTwo = [](glm::vec2 a, glm::vec2 b) { return Circle((a + b) * 0.5f, glm::distance(a, b) * 0.5f); };
		auto fromThree = [&](glm::vec2 a, glm::vec2 b, glm::vec2 c) {
			const glm::vec2 ab = b - a, ac = c - a;
			const float d = 2 * (ab.x * ac.y - ab.y * ac.x);
			if (std::abs(d) < 1e-9f)
			{
				Circle best = fromTwo(a, b);
				for (const auto &candidate : {fromTwo(a, c), fromTwo(b, c)})
				{
					if (candidate.second > best.second)
					{
						best = candidate;
					}
				}
				return best;
			}
			const glm::vec2 center{(ac.y * glm::dot(ab, ab) - ab.y * glm::dot(ac, ac)) / d,
								   (ab.x * glm::dot(ac, ac) - ac.x * glm::dot(ab, ab)) / d};
			return Circle(a + center, glm::length(center));
		};
		auto holds = [](const Circle &circle, glm::vec2 point) {
			return glm::distance(point, circle.first) <= circle.second * (1 + 1e-6f) + 1e-6f;
		};

		Circle circle{points[0], 0.0f};
		for (size_t i = 1; i < points.size(); i++)
		{
			if (holds(circle, points[i]))
			{
				continue;
			}
			circle = {points[i], 0.0f};
			for (size_t j = 0; j < i; j++)
			{
				if (holds(circle, points[j]))
				{
					continue;
				}
				circle = fromTwo(points[i], points[j]);
				for (size_t k = 0; k < j; k++)
				{
					if (!holds(circle, points[k]))
					{
						circle = fromThree(points[i], points[j], points[k]);
					}
				}
			}
		}
		return circle;
	}

	/*! Cylinder or capsule along the rotation's Z axis */
	Hitbox fitRound(const std::vector<glm::vec3> &points, const glm::quat &rotation, HitboxShape shape)
	{
		std::vector<glm::vec3> local;
		std::vector<glm::vec2> flat;
		for (const auto &point : points)
		{
			local.push_back(glm::inverse(rotation) * point);
			flat.emplace_back(local.back());
		}
		const auto [center, minRadius] = minCircle(flat);

		// A capsule can be shorter with a wider radius: its end caps hold more
		Hitbox best;
		float bestVolume = MAX_FLOAT;
		const int steps = shape == HitboxShape::Capsule ? 60 : 1;
		for (int step = 0; step < steps; step++)
		{
			const float radius = minRadius * (1.0f + 0.01f * float(step));
			float low = MAX_FLOAT, high = -MAX_FLOAT;
			for (const auto &point : local)
			{
				const float distance = glm::distance(glm::vec2(point), center);
				const float cap = shape == HitboxShape::Capsule ? std::sqrt(std::max(radius * radius - distance * distance, 0.0f)) : 0.0f;
				low = std::min(low, point.z + cap);
				high = std::max(high, point.z - cap);
			}

			Hitbox hitbox;
			hitbox.shape = shape;
			hitbox.radius = radius;
			// Capsule: low..high is the straight part (empty if the caps meet), cylinder: the whole height
			hitbox.height = shape == HitboxShape::Capsule ? std::max(high - low, 0.0f) + 2 * radius : high - low;
			hitbox.location = rotation * glm::vec3(center, (low + high) / 2);
			hitbox.rotation = rotation;
			if (volume(hitbox) < bestVolume)
			{
				bestVolume = volume(hitbox);
				best = hitbox;
			}
		}
		return best;
	}

	Hitbox fitBox(const std::vector<glm::vec3> &points, const glm::quat &rotation)
	{
		glm::vec3 low(MAX_FLOAT), high(-MAX_FLOAT);
		for (const auto &point : points)
		{
			const glm::vec3 local = glm::inverse(rotation) * point;
			low = glm::min(low, local);
			high = glm::max(high, local);
		}

		Hitbox hitbox;
		hitbox.shape = HitboxShape::Box;
		hitbox.location = rotation * ((low + high) * 0.5f);
		hitbox.rotation = rotation;
		hitbox.min = (low - high) * 0.5f;
		hitbox.max = (high - low) * 0.5f;
		return hitbox;
	}

	/*! Bounding sphere (Badoiu-Clarkson iterations, then the farthest point as the radius) */
	Hitbox fitSphere(const std::vector<glm::vec3> &points)
	{
		glm::vec3 center = points[0];
		for (int i = 1; i < 20000; i++)
		{
			const auto farthest = std::max_element(points.begin(), points.end(), [&](const glm::vec3 &a, const glm::vec3 &b) {
				return glm::distance(a, center) < glm::distance(b, center);
			});
			center += (*farthest - center) / float(i + 1);
		}

		Hitbox hitbox;
		hitbox.shape = HitboxShape::Sphere;
		hitbox.location = center;
		for (const auto &point : points)
		{
			hitbox.radius = std::max(hitbox.radius, glm::distance(point, center));
		}
		return hitbox;
	}

	/*! Principal axes of the points, the longest first */
	glm::mat3 principalAxes(const std::vector<glm::vec3> &points)
	{
		glm::vec3 mean(0.0f);
		for (const auto &point : points)
		{
			mean += point;
		}
		mean /= float(points.size());

		glm::mat3 covariance(0.0f);
		for (const auto &point : points)
		{
			covariance += glm::outerProduct(point - mean, point - mean);
		}

		// Power iteration, removing each found axis
		glm::mat3 axes(1.0f);
		glm::mat3 rest = covariance;
		for (int k = 0; k < 2; k++)
		{
			glm::vec3 axis = glm::normalize(glm::vec3(1.0f, 0.3f, 0.1f) + glm::vec3(float(k)));
			for (int i = 0; i < 200; i++)
			{
				axis = glm::normalize(rest * axis);
			}
			axes[k] = axis;
			rest -= glm::dot(axis, covariance * axis) * glm::outerProduct(axis, axis);
		}
		axes[1] = glm::normalize(axes[1] - axes[0] * glm::dot(axes[0], axes[1]));
		axes[2] = glm::cross(axes[0], axes[1]);
		return axes;
	}

	/*! Turn the rotation in shrinking steps while the fitted volume gets smaller, from each start rotation */
	Hitbox smallestRotation(const std::vector<glm::quat> &starts, const std::function<Hitbox(const glm::quat &)> &fit)
	{
		Hitbox best;
		float bestVolume = MAX_FLOAT;
		for (const auto &start : starts)
		{
			glm::quat rotation = start;
			Hitbox hitbox = fit(rotation);
			float current = volume(hitbox);
			for (float step = 0.4f; step > 0.0005f; step *= 0.6f)
			{
				bool improved = true;
				while (improved)
				{
					improved = false;
					for (int axis = 0; axis < 3; axis++)
					{
						for (float sign : {-1.0f, 1.0f})
						{
							glm::vec3 around(0.0f);
							around[axis] = 1.0f;
							const glm::quat candidate = glm::normalize(glm::angleAxis(sign * step, around) * rotation);
							const Hitbox fitted = fit(candidate);
							if (volume(fitted) < current - 1e-6f)
							{
								current = volume(fitted);
								hitbox = fitted;
								rotation = candidate;
								improved = true;
							}
						}
					}
				}
			}
			if (current < bestVolume)
			{
				bestVolume = current;
				best = hitbox;
			}
		}
		return best;
	}

	/*! One point per grid cell, for searching the rotation fast; the final size uses every point */
	std::vector<glm::vec3> thinned(const std::vector<glm::vec3> &points, float cell)
	{
		std::map<std::tuple<int, int, int>, glm::vec3> cells;
		for (const auto &point : points)
		{
			const glm::ivec3 key = glm::ivec3(glm::floor(point / cell));
			cells.emplace(std::make_tuple(key.x, key.y, key.z), point);
		}
		std::vector<glm::vec3> out;
		out.reserve(cells.size());
		for (const auto &entry : cells)
		{
			out.push_back(entry.second);
		}
		return out;
	}

	/*! How closely the hitboxes follow the mesh */
	struct Fit {
		float coverage = 100.0f; ///< percent of the vertex positions of a part that its hitbox must hold, the farthest ones are left out
		float margin = 0.05f;	 ///< space between the hitbox and the held vertices, negative goes inside the mesh
	};

	/*! How far out a point (in the hitbox's parent space) is: 1 on the surface, below 1 inside */
	float outsideRatio(const Hitbox &hitbox, const glm::vec3 &point)
	{
		const glm::vec3 local = glm::inverse(hitbox.rotation) * (point - hitbox.location);
		auto ratio = [](float value, float size) { return size > 1e-6f ? value / size : (value > 1e-6f ? MAX_FLOAT : 0.0f); };
		switch (hitbox.shape)
		{
		case HitboxShape::Sphere: return ratio(glm::length(local), hitbox.radius);
		case HitboxShape::Cylinder:
			return std::max(ratio(glm::length(glm::vec2(local)), hitbox.radius), ratio(std::abs(local.z), hitbox.height / 2));
		case HitboxShape::Capsule: {
			const float half = std::max(hitbox.height / 2 - hitbox.radius, 0.0f);
			return ratio(glm::distance(local, glm::vec3(0, 0, std::clamp(local.z, -half, half))), hitbox.radius);
		}
		case HitboxShape::Box:
		default: {
			// The boxes are centred on their location
			float out = 0.0f;
			for (int axis = 0; axis < 3; axis++)
			{
				out = std::max(out, ratio(std::abs(local[axis]), hitbox.max[axis]));
			}
			return out;
		}
		}
	}

	/**
	 * Fit a shape to the points, leaving out the farthest ones down to the coverage: the shape is fitted to every point,
	 * the points farthest out of it are dropped, and the shape is fitted again to the rest (a few rounds)
	 */
	Hitbox
	fitCovering(const std::vector<glm::vec3> &points, float coverage, const std::function<Hitbox(const std::vector<glm::vec3> &)> &fit)
	{
		Hitbox hitbox = fit(points);
		const size_t keep = std::clamp(size_t(std::ceil(double(points.size()) * double(coverage) / 100.0)), size_t(1), points.size());
		if (keep >= points.size())
		{
			return hitbox;
		}

		std::vector<std::pair<float, size_t>> ratios(points.size());
		for (int round = 0; round < 3; round++)
		{
			for (size_t i = 0; i < points.size(); i++)
			{
				ratios[i] = {outsideRatio(hitbox, points[i]), i};
			}
			std::nth_element(ratios.begin(), ratios.begin() + std::ptrdiff_t(keep - 1), ratios.end());
			std::vector<glm::vec3> kept;
			kept.reserve(keep);
			for (size_t i = 0; i < keep; i++)
			{
				kept.push_back(points[ratios[i].second]);
			}
			hitbox = fit(kept);
		}
		return hitbox;
	}

	/*! Grow (or with a negative margin shrink) the hitbox around the held points */
	void addMargin(Hitbox &hitbox, float margin)
	{
		hitbox.radius = std::max(hitbox.radius + margin, 0.01f);
		if (hitbox.shape != HitboxShape::Sphere)
		{
			hitbox.height = std::max(hitbox.height + 2 * margin, 0.02f);
		}
		hitbox.min = glm::min(hitbox.min - margin, glm::vec3(-0.01f));
		hitbox.max = glm::max(hitbox.max + margin, glm::vec3(0.01f));
		hitbox.clampSize();
	}

	/*! Head box: its bottom stays on the head bone's plane (X = 0), so it is only turned around the bone's X axis */
	Hitbox fitHead(const std::vector<glm::vec3> &points, const Fit &fit)
	{
		Hitbox hitbox = fitCovering(points, fit.coverage, [](const std::vector<glm::vec3> &held) {
			const std::vector<glm::vec3> few = thinned(held, 0.5f);
			glm::quat best{1, 0, 0, 0};
			float bestVolume = MAX_FLOAT;
			for (int step = 0; step < 360; step++)
			{
				const glm::quat rotation = glm::angleAxis(glm::radians(float(step) * 0.5f), glm::vec3(1, 0, 0));
				const float current = volume(fitBox(few, rotation));
				if (current < bestVolume)
				{
					bestVolume = current;
					best = rotation;
				}
			}
			return fitBox(held, best);
		});
		std::cout << "head (" << points.size() << " points): box " << int(volume(hitbox)) << ", from the head bone up\n";
		addMargin(hitbox, fit.margin);
		return hitbox;
	}

	/*! The smallest of the four shapes around the points, or the smallest capsule */
	Hitbox fitSmallest(const std::vector<glm::vec3> &points, const std::string &name, bool capsuleOnly, const Fit &fit)
	{
		// The rotations are searched with the thinned points, then each shape is sized to hold every point
		auto starts = [](const std::vector<glm::vec3> &few, bool box) {
			const glm::mat3 axes = principalAxes(few);
			auto along = [](const glm::vec3 &axis) { return glm::rotation(glm::vec3(0, 0, 1), glm::normalize(axis)); };
			if (box)
			{
				return std::vector<glm::quat>{glm::quat(1, 0, 0, 0), glm::normalize(glm::quat_cast(axes))};
			}
			return std::vector<glm::quat>{along(axes[0]), along({1, 0, 0}), along({0, 1, 0}), along({0, 0, 1})};
		};
		auto round = [&](HitboxShape shape) {
			return [&, shape](const std::vector<glm::vec3> &held) {
				const std::vector<glm::vec3> few = thinned(held, 0.5f);
				const glm::quat rotation
					= smallestRotation(starts(few, false), [&](const glm::quat &q) { return fitRound(few, q, shape); }).rotation;
				return fitRound(held, rotation, shape);
			};
		};

		std::vector<Hitbox> candidates;
		if (!capsuleOnly)
		{
			candidates.push_back(fitCovering(points, fit.coverage, [&](const std::vector<glm::vec3> &held) {
				const std::vector<glm::vec3> few = thinned(held, 0.5f);
				const glm::quat rotation = smallestRotation(starts(few, true), [&](const glm::quat &q) { return fitBox(few, q); }).rotation;
				return fitBox(held, rotation);
			}));
			candidates.push_back(fitCovering(points, fit.coverage, [](const std::vector<glm::vec3> &held) {
				Hitbox sphere = fitSphere(thinned(held, 0.5f));
				for (const auto &point : held)
				{
					sphere.radius = std::max(sphere.radius, glm::distance(point, sphere.location));
				}
				return sphere;
			}));
			candidates.push_back(fitCovering(points, fit.coverage, round(HitboxShape::Cylinder)));
		}
		candidates.push_back(fitCovering(points, fit.coverage, round(HitboxShape::Capsule)));

		std::cout << name << " (" << points.size() << " points):";
		Hitbox best = candidates.back();
		for (const auto &candidate : candidates)
		{
			std::cout << " " << hitboxShapeName(candidate.shape) << " " << int(volume(candidate));
			if (volume(candidate) < volume(best))
			{
				best = candidate;
			}
		}
		std::cout << " -> " << hitboxShapeName(best.shape) << "\n";

		addMargin(best, fit.margin);
		return best;
	}

	glm::vec3 toLocal(const Point &joint, const glm::vec3 &point)
	{
		return glm::inverse(joint.rotation) * (point - joint.location);
	}

	glm::vec3 fromLocal(const Point &joint, const glm::vec3 &point)
	{
		return joint.location + joint.rotation * point;
	}

	int usage()
	{
		std::cerr
			<< "Usage: fit_hitboxes <base folder> <character.char>... <output.hitboxes> [--single-pose [--sequence name | --frame n]]\n"
			   "                    [--coverage percent] [--margin units] [--skip-coat]\n"
			   "  Several characters (with the same bones) give one hitbox set that fits all of them\n"
			   "  --coverage  percent of each part's vertex positions its hitbox must hold (default 100), the farthest ones\n"
			   "              are left out, so a lower value gives smaller hitboxes that let some of the mesh out\n"
			   "  --margin    space between the hitboxes and the vertices they hold (default 0.05), negative values fit\n"
			   "              inside the mesh\n"
			   "  --skip-coat leave out the coat: upper body mesh vertices on the leg bones or more than 2 units below the\n"
			   "              hip joints in the fitting pose\n";
		return 1;
	}

	/*! A character to fit: its body (with the animation group), head and the hitbox group of every body vertex */
	struct Character {
		std::string path;
		std::unique_ptr<Model> loaded;
		skeletal::SkeletalModel *body = nullptr;
		std::unique_ptr<Model> head;
		std::vector<std::vector<int>> vertexGroups;
	};
}

int main(int argc, char **argv)
{
	std::string sequence = "alert_idle_2h";
	int frame = -1;
	bool allFrames = true;
	bool skipCoat = false;
	Fit fit;
	std::vector<std::string> positional;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		if (arg == "--sequence" && i + 1 < argc)
		{
			sequence = argv[++i];
		}
		else if (arg == "--frame" && i + 1 < argc)
		{
			frame = std::atoi(argv[++i]);
		}
		else if (arg == "--coverage" && i + 1 < argc)
		{
			fit.coverage = std::strtof(argv[++i], nullptr);
			if (!(fit.coverage > 0.0f && fit.coverage <= 100.0f))
			{
				std::cerr << "The coverage is a percent above 0 and up to 100\n";
				return 1;
			}
		}
		else if (arg == "--margin" && i + 1 < argc)
		{
			fit.margin = std::strtof(argv[++i], nullptr);
		}
		else if (arg == "--skip-coat")
		{
			skipCoat = true;
		}
		else if (arg == "--single-pose")
		{
			allFrames = false;
		}
		else if (arg.rfind("--", 0) == 0)
		{
			return usage();
		}
		else
		{
			positional.push_back(arg);
		}
	}
	if (positional.size() < 3)
	{
		return usage();
	}
	const std::string basePath = positional.front();
	const std::string output = positional.back();
	const std::vector<std::string> characterPaths(positional.begin() + 1, positional.end() - 1);

	try
	{
		mt::io::FileSystem files;
		if (!files.addPath(basePath))
		{
			throw mt_ex("Base folder not found: " + basePath);
		}
		auto text = [&](const std::string &path) {
			auto file = files.loadFile(path);
			if (!file)
			{
				throw mt_ex("File not found: " + path);
			}
			return fileText(file);
		};

		std::vector<Character> characters;
		for (const auto &path : characterPaths)
		{
			Character character;
			character.path = path;
			const auto def = parseCharacter(text(path));
			character.loaded.reset(mt::model::io::loadModel(files.loadFile(def.mesh)));
			character.body = dynamic_cast<skeletal::SkeletalModel *>(character.loaded.get());
			if (!character.body)
			{
				throw mt_ex("The body is not a skeletal model: " + def.mesh);
			}
			loadAnimationGroup(*character.body, parseAnimationGroup(text(def.animationGroup), text),
							   [&](const std::string &file) { return files.loadFile(file); });
			if (!def.hudHead.empty())
			{
				character.head.reset(mt::model::io::loadModel(files.loadFile(def.hudHead)));
			}

			// One hitbox file for all: the bones (their order) and the frames must be the same
			if (!characters.empty())
			{
				const auto *first = characters.front().body;
				bool same = first->joints.size() == character.body->joints.size() && first->numFrames() == character.body->numFrames();
				for (size_t j = 0; same && j < first->joints.size(); j++)
				{
					same = first->joints[j].name == character.body->joints[j].name;
				}
				if (!same)
				{
					throw mt_ex("The bones or frames of " + path + " differ from " + characters.front().path);
				}
			}
			std::cout << "Loaded " << path << "\n";
			characters.push_back(std::move(character));
		}
		const auto &reference = *characters.front().body;

		if (frame < 0)
		{
			auto found = std::find_if(reference.animations.begin(), reference.animations.end(),
									  [&](const Animation &a) { return a.name == sequence; });
			if (found == reference.animations.end())
			{
				throw mt_ex("Sequence not found: " + sequence);
			}
			frame = found->startFrame;
		}

		// The body parts, by the ET (Biped) bone names
		auto joint = [&](const std::string &name) {
			for (size_t i = 0; i < reference.joints.size(); i++)
			{
				if (reference.joints[i].name == name)
				{
					return int(i);
				}
			}
			throw mt_ex("Bone not found: " + name);
		};
		std::vector<Group> groups;
		groups.push_back({"head", joint("Bip01 Head"), {joint("Bip01 Head")}, true});
		groups.push_back({"torso",
						  joint("Bip01 Spine2"),
						  {joint("Bip01 Pelvis"), joint("Bip01 Spine"), joint("Bip01 Spine1"), joint("Bip01 Spine2"), joint("Bip01 Spine3"),
						   joint("Bip01 Neck"), joint("Bip01 L Clavicle"), joint("Bip01 R Clavicle")}});
		for (const std::string side : {"L", "R"})
		{
			const std::string bone = "Bip01 " + side + " ";
			const std::string suffix = side == "L" ? "_left" : "_right";

			// The forearm takes the hand and every finger bone under it
			std::vector<int> forearm = {joint(bone + "Forearm"), joint(bone + "Hand")};
			for (size_t i = 0; i < reference.joints.size(); i++)
			{
				if (reference.joints[i].name.rfind(bone + "Finger", 0) == 0)
				{
					forearm.push_back(int(i));
				}
			}

			groups.push_back({"upper_arm" + suffix, joint(bone + "UpperArm"), {joint(bone + "UpperArm")}, false, true});
			groups.push_back({"forearm" + suffix, joint(bone + "Forearm"), forearm, false, true});
			groups.push_back({"thigh" + suffix, joint(bone + "Thigh"), {joint(bone + "Thigh")}, false, true});
			groups.push_back({"calf" + suffix, joint(bone + "Calf"), {joint(bone + "Calf")}, false, true});
			groups.push_back({"foot" + suffix, joint(bone + "Foot"), {joint(bone + "Foot"), joint(bone + "Toe0")}});
		}

		std::map<int, int> groupOfJoint;
		for (size_t g = 0; g < groups.size(); g++)
		{
			for (int j : groups[g].joints)
			{
				groupOfJoint[j] = int(g);
			}
		}

		// The group of every body vertex, by its strongest bone. -1 leaves the vertex out: with --skip-coat the vertices of
		// the upper body meshes (not l_*) that are on the leg bones or hang more than 2 units below the hip joints in the
		// fitting pose (the skirt of a long coat or jacket)
		auto legBone = [&](int j) {
			const std::string &name = reference.joints[size_t(j)].name;
			for (const char *part : {"Thigh", "Calf", "Foot", "Toe"})
			{
				if (name.find(part) != std::string::npos)
				{
					return true;
				}
			}
			return false;
		};
		for (auto &character : characters)
		{
			PoseOptions options;
			options.frame = frame;
			options.nextFrame = frame;
			const auto posed = poseModel(*character.body, options);
			auto bonePosition = [&](const std::string &name) { return posed.bonePoints.at(size_t(joint(name))).point.location; };
			const glm::vec3 up = glm::normalize(bonePosition("Bip01 Spine3") - bonePosition("Bip01 Pelvis"));
			const float hip = (glm::dot(bonePosition("Bip01 L Thigh"), up) + glm::dot(bonePosition("Bip01 R Thigh"), up)) / 2;

			size_t skipped = 0;
			for (size_t m = 0; m < character.body->meshes.size(); m++)
			{
				const auto &mesh = character.body->meshes[m];
				character.vertexGroups.emplace_back();
				for (size_t v = 0; v < mesh.vertices.size(); v++)
				{
					const auto &vertex = mesh.vertices[v];
					int strongest = -1;
					float bias = -1.0f;
					for (int i = 0; i < vertex.weightCount; i++)
					{
						const auto &weight = mesh.weights.at(size_t(vertex.startWeight + i));
						if (weight.bias > bias)
						{
							bias = weight.bias;
							strongest = weight.jointId;
						}
					}
					auto found = groupOfJoint.find(strongest);
					if (found == groupOfJoint.end())
					{
						throw mt_ex("No hitbox for the vertices of bone " + (strongest >= 0 ? reference.joints[strongest].name : "-"));
					}
					if (skipCoat && mesh.name.rfind("l_", 0) != 0
						&& (legBone(strongest) || glm::dot(posed.meshes[m].positions[v], up) < hip - 2.0f))
					{
						character.vertexGroups.back().push_back(-1);
						skipped++;
						continue;
					}
					character.vertexGroups.back().push_back(found->second);
				}
			}
			if (skipped > 0)
			{
				std::cout << "Left out " << skipped << " coat vertices of " << character.path << "\n";
			}
		}

		// The points of every group in a pose, in the space of the group's bone
		auto collect = [&](const Character &character, int poseFrame, std::vector<std::vector<glm::vec3>> &points) {
			PoseOptions options;
			options.frame = poseFrame;
			options.nextFrame = poseFrame;
			const auto posed = poseModel(*character.body, options);
			auto bone = [&](size_t g) { return posed.bonePoints.at(size_t(groups[g].parent)).point; };

			for (size_t m = 0; m < posed.meshes.size(); m++)
			{
				for (size_t v = 0; v < posed.meshes[m].positions.size(); v++)
				{
					if (character.vertexGroups[m][v] < 0)
					{
						continue;
					}
					const size_t g = size_t(character.vertexGroups[m][v]);
					points[g].push_back(toLocal(bone(g), posed.meshes[m].positions[v]));
				}
			}

			// The head model on the body's tag_head, like the game draws it. Its neck (below the head bone: the bone
			// points up along its X axis) is held by the torso like the body's neck
			glm::mat4 tag{1.0f};
			if (character.head && tagMatrix(posed, "tag_head", tag))
			{
				for (const auto &mesh : poseModel(*character.head, PoseOptions{}).meshes)
				{
					for (const auto &position : mesh.positions)
					{
						const glm::vec3 point = glm::vec3(tag * glm::vec4(position, 1.0f));
						const glm::vec3 local = toLocal(bone(0), point);
						if (local.x < 0.0f)
						{
							points[1].push_back(toLocal(bone(1), point));
						}
						else
						{
							points[0].push_back(local);
						}
					}
				}
			}
		};

		std::cout << "Fitting to " << (allFrames ? "every 5th frame" : "frame " + std::to_string(frame)) << " of " << characters.size()
				  << " character(s), coverage " << fit.coverage << "%, margin " << fit.margin << "\n";
		std::vector<std::vector<glm::vec3>> points(groups.size());
		for (const auto &character : characters)
		{
			collect(character, frame, points);
			if (allFrames)
			{
				for (int f = 0; f < reference.numFrames(); f += 5)
				{
					collect(character, f, points);
				}
			}
		}

		std::vector<Hitbox> hitboxes;
		for (size_t g = 0; g < groups.size(); g++)
		{
			Hitbox hitbox = groups[g].head ? fitHead(points[g], fit) : fitSmallest(points[g], groups[g].name, groups[g].capsule, fit);
			hitbox.name = groups[g].name;
			hitbox.parentJoint = groups[g].parent;
			if (groups[g].head)
			{
				hitbox.modifiers.emplace("headshot");
			}
			const size_t held
				= size_t(std::count_if(points[g].begin(), points[g].end(), [&](const glm::vec3 &point) { return inside(hitbox, point); }));
			std::cout << "  holds " << 100.0 * double(held) / double(points[g].size()) << "% of its points\n";
			if (fit.coverage >= 100.0f && fit.margin >= 0.0f && held < points[g].size())
			{
				throw mt_ex("The fitted " + hitbox.name + " hitbox misses a point");
			}
			hitboxes.push_back(hitbox);
		}
		points.clear();
		points.shrink_to_fit();

		// How much of each mesh the hitboxes hold in every 5th frame (vertices blended between bones can move out)
		float sum = 0.0f;
		for (const auto &hitbox : hitboxes)
		{
			sum += volume(hitbox);
		}
		std::cout << "Volumes:\n";
		for (const auto &hitbox : hitboxes)
		{
			std::cout << "  " << hitbox.name << " (" << hitboxShapeName(hitbox.shape) << "): " << int(std::lround(volume(hitbox))) << "\n";
		}
		std::cout << "Total volume of the " << hitboxes.size() << " hitboxes: " << int(std::lround(sum)) << " cubic units\n";
		std::cout << "Vertex positions inside a hitbox over every 5th frame" << (skipCoat ? " (coat vertices left out)" : "") << ":\n";
		size_t allTotal = 0, allMissed = 0;
		for (const auto &character : characters)
		{
			size_t total = 0, missed = 0;
			for (int f = 0; f < reference.numFrames(); f += 5)
			{
				std::vector<std::vector<glm::vec3>> posePoints(groups.size());
				collect(character, f, posePoints);
				PoseOptions options;
				options.frame = f;
				options.nextFrame = f;
				const auto posed = poseModel(*character.body, options);
				for (size_t g = 0; g < groups.size(); g++)
				{
					for (const auto &point : posePoints[g])
					{
						const glm::vec3 position = fromLocal(posed.bonePoints[size_t(groups[g].parent)].point, point);
						total++;
						bool held = false;
						for (const auto &hitbox : hitboxes)
						{
							held = held || inside(hitbox, toLocal(posed.bonePoints[size_t(hitbox.parentJoint)].point, position));
						}
						missed += held ? 0 : 1;
					}
				}
			}
			std::cout << "  " << character.path << ": " << 100.0 * double(total - missed) / double(total) << "%\n";
			allTotal += total;
			allMissed += missed;
		}
		if (characters.size() > 1)
		{
			std::cout << "  all: " << 100.0 * double(allTotal - allMissed) / double(allTotal) << "%\n";
		}

		Hitbox::saveHitboxes(output, hitboxes);
		std::ofstream note(output, std::ios::app);
		note << "// Total volume " << int(std::lround(sum)) << " cubic units, coverage " << fit.coverage << "%, margin " << fit.margin
			 << (skipCoat ? ", coat left out" : "") << ", inside " << 100.0 * double(allTotal - allMissed) / double(allTotal) << "%\n";
		std::cout << "Saved " << output << " (total volume " << int(std::lround(sum)) << ")\n";
	}
	catch (const std::exception &ex)
	{
		std::cerr << ex.what() << "\n";
		return 1;
	}
	return 0;
}
