#pragma once

#include "mt.h"
#include "model.h"
#include "model_md3.h"
#include "morph_model.h"
#include "filesystem.h"
#include "measure.h"
#include "playback.h"
#include "texture_cache.h"
#include "settings.h"
#include "ui_theme.h"
#include "player.h"
#include "skin.h"
#include "et_character.h"

#include <filesystem>
#include <optional>

namespace mt {

	/*! What the move / rotate gizmo of the scene edits */
	enum class EditTarget
	{
		None,
		Tag,   ///< ViewSettings::selectedTag
		Hitbox ///< ViewSettings::selectedHitbox
	};

	/*! What the edit gizmo does */
	enum class GizmoMode
	{
		Move,
		Rotate,
		Scale ///< hitboxes only
	};

	/*! What is behind the model in the scene */
	enum class Background
	{
		Color,	  ///< one color
		Gradient, ///< from the background color at the top to a second color at the bottom
		SkyBox	  ///< a sky box of the game (a shader's skyParms)
	};

	/*! What is drawn in the scene and how */
	struct ViewSettings {
		bool wireframe = false;									///< draw triangle edges only
		bool textures = true;									///< draw the textures
		bool alpha = false;										///< blend with the texture alpha
		bool shaders = true;									///< draw like the shader scripts say: cull, alpha test, blending
		bool twoSided = false;									///< draw both sides of every triangle (no culling)
		bool flatShading = false;								///< light each triangle with its own normal
		bool drawMeasurements = false;							///< width, depth and height lines of the whole model (attachments too)
		model::LengthUnit lengthUnit = model::LengthUnit::Game; ///< units of the measurements
		float gamma = 1.0f;										///< like the game's r_gamma (1 = unchanged)
		float intensity = 1.0f;									///< like the game's r_intensity (1 = unchanged)
		bool drawGrid = true;									///< ground grid and origin axis
		bool drawBounds = false;								///< frame bounding box
		bool drawHitboxes = true;								///< hitboxes of skeletal models
		bool drawGizmo = true;									///< orientation gizmo in the corner of the scene
		int selectedHitbox = -1;								///< hitbox drawn highlighted (open in the hitbox panel)
		int selectedTag = -1;									///< tag selected for the gizmo
		EditTarget editTarget = EditTarget::None;				///< what the gizmo moves
		GizmoMode gizmoMode = GizmoMode::Move;					///< what the edit gizmo does
		bool gizmoLocal = true;									///< the gizmo axes follow the edited object instead of the world
		bool gizmoAllFrames = true;								///< moving a morph model tag moves it in every frame
		bool drawTags = false;									///< tag axis
		bool drawTagNames = true;								///< names next to the tags
		bool drawSkeleton = false;								///< bones and joints
		bool drawNormals = false;								///< vertex normals
		float normalLength = 0.03f;								///< length of the drawn normals, relative to the model size
		int lodLevel = 0;										///< level of detail of morph models with extra levels
		bool dynamicLod = false;								///< reduce triangles with the collapse maps
		float lodRatio = 0.0f;									///< dynamic level of detail: 0 = minimum LOD, 1 = full mesh
		int picmip = 0;											///< texture picmip (halvings)
		TextureFilter textureFilter = TextureFilter::Trilinear;
	};

	/*! Model drawn on a tag of the main model or of another attachment */
	struct Attachment {
		Ref<Model> model = nullptr; ///< attached model
		std::string path{};			///< file of the model (textures are looked up next to it)
		std::string name{};			///< shown name
		int parent = -1;			///< -1 for the main model, else the index of an earlier attachment
		std::string tag{};			///< tag of the parent the model follows
		model::Playback playback{}; ///< frames of the attached model, played along with the main playback
		model::Skin skin{};			///< shaders of the meshes
	};

	/*! A tag or bone picked in the panels to attach a model to, the menu view opens the file dialog for it */
	struct AttachRequest {
		int parent = -1;	   ///< -1 for the main model, else the attachment index
		std::string point{};   ///< tag or bone name
		bool fromBase = false; ///< pick the model from the base folders instead of the disk
	};

	/*! State singleton object */
	class State {
		friend Ref<State> singleton_ref<State>();

	  public:
		Ref<Model> model = nullptr;					   ///< currently loaded model
		ImColor clearColor{0.15f, 0.16f, 0.18f};	   ///< scene background color (the top of the gradient)
		Background background = Background::Color;	   ///< what is behind the model
		ImColor backgroundBottom{0.55f, 0.58f, 0.62f}; ///< bottom color of the gradient background
		std::string skyBox{};						   ///< sky box of the shaders (skyParms) for Background::SkyBox
		ui::Theme theme = ui::Theme::Darker;		   ///< user interface colors
		float fontSize = ui::DEFAULT_FONT_SIZE;		   ///< user interface font size
		io::FileSystem filesystem{};				   ///< used filesystem, which will be initialized by the ImGuiView
		std::string modelPath{};					   ///< path of the loaded model, textures are also looked up next to it
		ViewSettings view{};						   ///< scene drawing settings
		model::Playback playback{};					   ///< animation playback
		TextureCache textures{};					   ///< loaded textures
		bool frameCamera = false;					   ///< the scene panel should point the camera at the model
		bool resetCamera = false;					   ///< the scene panel should reset the camera
		AppSettings settings{};						   ///< settings kept between runs
		bool persistSettings = false;				   ///< load and save the settings (off for screenshots)
		std::vector<std::string> basePaths{};		   ///< base folders added by the user
		bool showLog = true;						   ///< the log panel is open
		model::Skin skin{};							   ///< skin of the main model
		std::vector<Attachment> attachments{};		   ///< models drawn on tags, parents come before their children
		bool isPlayer = false;						   ///< a three part player model is loaded (lower is the main model)
		model::PlayerFiles player{};				   ///< files of the player model
		model::PlayerConfig playerConfig{};			   ///< animation.cfg of the player model
		std::optional<AttachRequest> attachRequest{};  ///< attach a model to this tag or bone (handled by the menu view)
		std::string playerSkin{};					   ///< skin name of the player model
		bool isCharacter = false;					   ///< an ET character (.char) is loaded (the body is the main model)
		std::string characterPath{};				   ///< the .char file
		model::CharacterDef character{};			   ///< the parsed .char file
		bool characterUndressed = false;			   ///< the undressed corpse model and skin are shown

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
		 * Uses the filesystem and the modelloader to load a model into the current state. animation.cfg of a player
		 * model folder loads the player (lower, upper and head)
		 * @param path path to the model
		 */
		void loadModel(const std::string &path);

		/**
		 * Load a three part player model: lower is the main model, upper on its tag_torso and head on the upper's
		 * tag_head, with the default skin and the idle sequences
		 * @param files player files
		 */
		void loadPlayer(const model::PlayerFiles &files);

		/**
		 * Load an ET character: the body mesh with the animation group and the skin, the hud head on tag_head with its
		 * skin and animations, and the skin's accessories on their tags (like the game draws a player)
		 * @param path .char file
		 * @param undressed show the undressed corpse model and skin instead (no accessories, like a looted corpse)
		 */
		void loadCharacter(const std::string &path, bool undressed = false);

		/**
		 * Use another skin of the player model
		 * @param name skin name (lower_<name>.skin, ...)
		 */
		void setPlayerSkin(const std::string &name);

		/**
		 * Use a skin file for the main model
		 * @param path skin file in the base folders or on the disk
		 * @throws mt::mt_exception if the file is missing
		 */
		void loadModelSkin(const std::string &path);

		/**
		 * Write the skins into the models (the main model and every attachment with its own skin), so they are
		 * exported with the skin's shaders; the skins' surface lines are cleared afterwards
		 * @return number of meshes whose material changed
		 */
		size_t bakeSkins();

		/**
		 * @return the main model or an attachment has a skin with surface shaders
		 */
		[[nodiscard]] bool hasSkins() const;

		/**
		 * Play a sequence on the main model (-1) or an attachment
		 * @param target -1 for the main model, else the attachment index
		 * @param animation sequence
		 */
		void playSequence(int target, const model::Animation &animation);

		/**
		 * Attach a model to a tag
		 * @param path model file
		 * @param parent -1 for the main model, else the index of an attachment
		 * @param tag tag of the parent
		 * @return the new attachment
		 * @throws mt::mt_exception if the model can not be loaded
		 */
		Attachment &attachModel(const std::string &path, int parent, const std::string &tag);

		/**
		 * Remove an attachment and the attachments on it
		 * @param index attachment index
		 */
		void removeAttachment(int index);

		/**
		 * @param target -1 for the main model, else an attachment index
		 * @return tag names of the model (tags, and bones usable as tags of skeletal models)
		 */
		[[nodiscard]] std::vector<std::string> tagNames(int target) const;

		/**
		 * @param target -1 for the main model, else an attachment index
		 * @return where models can be attached: the tags, then the other bones of skeletal models
		 */
		[[nodiscard]] std::vector<std::string> attachPoints(int target) const;

	  private:
		/**
		 * Load a model file as the main model
		 * @param path path to the model
		 * @param recent add it to the recent files
		 */
		void loadSingleModel(const std::string &path, bool recent)
		{
			closeModels();
			model = loadModelFile(path);

			modelPath = path;
			if (recent)
			{
				settings.addRecentFile(path);
			}
			view.lodLevel = 0;
			view.selectedHitbox = -1;
			view.selectedTag = -1;
			view.editTarget = EditTarget::None;
			frameCamera = true;
			resetPlayback();
		}

		/**
		 * Load a model with its levels of detail (morph models)
		 */
		Ref<Model> loadModelFile(const std::string &path);

	  public:
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
			textures.loadShaders(filesystem);
			return true;
		}

		/**
		 * Remove every base folder
		 */
		void clearBasePaths()
		{
			filesystem.clear();
			basePaths.clear();
			textures.loadShaders(filesystem);
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
		void update(double seconds);

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
			attachments.clear();
			attachRequest.reset();
			skin = {};
			isPlayer = false;
			player = {};
			playerConfig = {};
			playerSkin.clear();
			isCharacter = false;
			characterPath.clear();
			character = {};
			characterUndressed = false;
		}

	  private:
		/**
		 * Hidden constructor for singleton (the friend function handles it)
		 */
		State() = default;
	};
}
