#pragma once

#include "mt.h"
#include "model.h"
#include "model_md3.h"
#include "morph_model.h"
#include "filesystem.h"
#include "playback.h"
#include "texture_cache.h"
#include "settings.h"
#include "ui_theme.h"

#include <filesystem>

namespace mt {

	/*! What is drawn in the scene and how */
	struct ViewSettings {
		bool wireframe = false;		///< draw triangle edges only
		bool textures = true;		///< draw the textures
		bool alpha = false;			///< blend with the texture alpha
		bool drawGrid = true;		///< ground grid and origin axis
		bool drawBounds = false;	///< frame bounding box
		bool drawHitboxes = true;	///< hitboxes of skeletal models
		int selectedHitbox = -1;	///< hitbox drawn highlighted (open in the hitbox panel)
		bool drawTags = false;		///< tag axis
		bool drawTagNames = true;	///< names next to the tags
		bool drawSkeleton = false;	///< bones and joints
		bool drawNormals = false;	///< vertex normals
		float normalLength = 0.03f; ///< length of the drawn normals, relative to the model size
		int lodLevel = 0;			///< level of detail of morph models with extra levels
		bool dynamicLod = false;	///< reduce triangles with the collapse maps
		float lodRatio = 0.0f;		///< dynamic level of detail: 0 = minimum LOD, 1 = full mesh
		int picmip = 0;				///< texture picmip (halvings)
		TextureFilter textureFilter = TextureFilter::Trilinear;
	};

	/*! State singleton object */
	class State {
		friend Ref<State> singleton_ref<State>();

	  public:
		Ref<Model> model = nullptr;				 ///< currently loaded model
		ImColor clearColor{0.15f, 0.16f, 0.18f}; ///< scene background color
		ui::Theme theme = ui::Theme::Darker;	 ///< user interface colors
		float fontSize = ui::DEFAULT_FONT_SIZE;	 ///< user interface font size
		io::FileSystem filesystem{};			 ///< used filesystem, which will be initialized by the ImGuiView
		std::string modelPath{};				 ///< path of the loaded model, textures are also looked up next to it
		ViewSettings view{};					 ///< scene drawing settings
		model::Playback playback{};				 ///< animation playback
		TextureCache textures{};				 ///< loaded textures
		bool frameCamera = false;				 ///< the scene panel should point the camera at the model
		bool resetCamera = false;				 ///< the scene panel should reset the camera
		AppSettings settings{};					 ///< settings kept between runs
		bool persistSettings = false;			 ///< load and save the settings (off for screenshots)
		std::vector<std::string> basePaths{};	 ///< base folders added by the user
		bool showLog = true;					 ///< the log panel is open

		/**
		 * no copy for singletons
		 */
		State(State const &) = delete;

		/**
		 * No assign for singletons
		 * @return delete
		 */
		State &operator=(State const &) = delete;

		~State()
		{
			clear();
		}

		/**
		 * Clear all values and models (possibly to have linked models in the future)
		 */
		void clear()
		{
			closeModels();
			textures.clear();
		}

		/**
		 * Uses the filesystem and the modelloader to load a model into the current state
		 * @param path path to the model
		 */
		void loadModel(const std::string &path)
		{
			closeModels();

			auto *tmp = model::io::loadModel(filesystem.loadFile(path));
			if (tmp)
			{
				model = std::unique_ptr<Model>(tmp);
			}

			if (auto *morph = dynamic_cast<model::morph::MorphModel *>(model.get()))
			{
				for (const auto &problem : model::loadMorphLods(*morph, filesystem, path))
				{
					if (auto logger = spdlog::get(MT_LOGGER))
					{
						logger->warn("{}", problem);
					}
				}
			}

			modelPath = path;
			settings.addRecentFile(path);
			view.lodLevel = 0;
			view.selectedHitbox = -1;
			frameCamera = true;
			resetPlayback();
		}

		/**
		 * Add a base folder (or a pk3 file) to load the models and textures from
		 * @param path folder or pk3 path
		 * @return false if the path does not exist
		 */
		bool addBasePath(const std::string &path)
		{
			if (!filesystem.addPath(path))
			{
				return false;
			}
			// Absolute, so the settings work from any working folder
			std::error_code error;
			std::string absolute = std::filesystem::absolute(path, error).lexically_normal().string();
			if (error)
			{
				absolute = path;
			}
			if (std::find(basePaths.begin(), basePaths.end(), absolute) == basePaths.end())
			{
				basePaths.push_back(absolute);
			}
			textures.clear();
			return true;
		}

		/**
		 * Remove every base folder
		 */
		void clearBasePaths()
		{
			filesystem.clear();
			basePaths.clear();
			textures.clear();
		}

		/**
		 * Set the playback range to every frame of the model and pause
		 */
		void resetPlayback()
		{
			playback.pause();
			const int frames = model ? model->numFrames() : 0;
			const float fps = (model && !model->animations.empty()) ? float(model->animations.front().fps) : 24.0f;
			playback.setRange(0, std::max(frames - 1, 0), fps, true);
		}

		/**
		 * Move the animation forward, called every frame
		 * @param seconds time since the last call
		 */
		void update(double seconds)
		{
			if (!model || !playback.isPlaying() || !model->hasFrames())
			{
				return;
			}

			playback.advance(seconds);
			model->currentFrame = std::clamp(playback.frame(), 0, model->numFrames() - 1);
		}

		/**
		 * Uses the filesystem and the modelloader to load an animation into the current state's model
		 * @param path path to the animation
		 */
		void loadAnimation(const std::string &path)
		{
			if (!model)
			{
				return;
			}

			try
			{
				model::io::loadAnimation(model.get(), filesystem.loadFile(path));
				resetPlayback();
			}
			catch (const std::exception &ex)
			{
				// The model is now possibly broken, so just close it
				closeModels();

				// rethrow the same exception forward
				throw;
			}
		}

		/**
		 * Clear models (possibly to have linked models in the future)
		 */
		void closeModels()
		{
			model = nullptr;
			modelPath.clear();
			textures.clear();
			playback.pause();
		}

	  private:
		/**
		 * Hidden constructor for singleton (the friend function handles it)
		 */
		State() = default;
	};
}
