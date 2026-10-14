# ModelTool [![CMake](https://github.com/jackeri/ModelTool/actions/workflows/cmake.yml/badge.svg)](https://github.com/jackeri/ModelTool/actions/workflows/cmake.yml)

> Jere 'Jacker' S. 2021

ModelTool is a viewer and editor for game models, mainly for Wolfenstein: Enemy Territory and the model formats of
the [ET:Legacy](https://www.etlegacy.com) project. It loads models the way the game does (from a base folder and the
`.pk3` files in it), shows them with their textures and animations, does the usual model fixes, and converts between
the formats.

It comes as a desktop application (`ModelTool`) and a command line tool (`mt`) built on the same library.

![ModelTool hitboxes](doc/hitboxes.png)

## Features

### Formats

| Format                       | Read | Write | Notes                                                                                      |
| ---------------------------- | ---- | ----- | ------------------------------------------------------------------------------------------ |
| MD3                          | yes  | yes   | Morph (vertex) animated models, levels of detail (`model_1.md3`, `model_2.md3`, ...), tags |
| MD3 tags only                |      | yes   | Tag animation without surfaces                                                             |
| MDC                          | yes  | yes   | Compressed MD3; writes full frames, or compressed frames as an option (library)                                                                             |
| MDM + MDX                    | yes  | yes   | ET skeletal mesh and animation                                                             |
| MDS                          | yes  | yes   | RtCW skeletal model                                                                        |
| MD5 (`.md5mesh`, `.md5anim`) | yes  | yes   |                                                                                            |
| IQM                          | yes  |       |                                                                                            |
| MS3D (MilkShape 3D)          | yes  | yes   | Mesh and animation, tags as tag bones, up to 4 weights per vertex                                                                         |
| FBX, OBJ                     | yes  |       | Through ufbx                                                                               |
| glTF, GLB                    | yes  | yes   | Read through tinygltf (meters to game units), with embedded images and colors; written in meters with the attached models, skins, animations, textures |
| STL                          |      | yes   | Binary or text                                                                             |
| `.aninc`                     | yes  | yes   | Animation sequences (ET animation scripts)                                                 |
| `.tag`                       | yes  | yes   | Tags file                                                                                  |
| `.hitboxes`                  | yes  | yes   | Hitboxes; old LegacyModelView `.htx` files are read too                                    |
| `.skin`                      | yes  |       | Shaders per mesh, Quake 3 and ET style; File > Load skin for any model, Edit > Bake skin   |
| `.shader`                    | yes  |       | Quake 3 / ET shader scripts in `scripts/`: image, cull, alphaFunc, blendFunc               |
| `animation.cfg`              | yes  |       | Quake 3 style player models                                                                |
| `.char`                      | yes  |       | ET characters, with their animation group (`.anim`) and head animations                    |

Every binary loader checks the file against its size, so broken files give an error instead of a crash.

### Files like the game

- **Base folders**: add the game's mod folder (like `etmain`), and its loose files, `.pk3dir` folders and `.pk3`
  files act as one file system. Several base folders can be added.
- **Search order** in a base folder: loose files first, then `.pk3dir` folders, then `.pk3` files, later package
  names first (`pak1.pk3` over `pak0.pk3`).
- **Upper and lower case** in file names are ignored like the game does (can be turned off in the settings).
- **Textures** are found from the shader names of the meshes: the shader script's image, else the name as it is, or
  with an image extension added (`.tga`, `.jpg`, `.png`, ...), next to the model or in the base folders. Picmip and
  texture filtering can be set.
- **Shader scripts**: every `scripts/*.shader` of the base folders is read like the game does (a later file replaces
  the shaders of an earlier one). A material that names a shader uses the shader's image (the first `map`,
  `clampMap`, `animMap` or `implicitMap` / `implicitMask` / `implicitBlend` stage, else `qer_editorimage`) and is
  drawn like it: `cull` (one or both sides), `alphaFunc` and `blendFunc`; `nopicmip` textures keep their size. The
  Meshes section shows each mesh's shader. View > Shader effects turns the culling, alpha test and blending off.
  animMap animation, environment maps, tcMod / rgbGen / deformVertexes and multi-stage blending are not drawn

### Viewing

- Textured, lit model with wireframe, alpha and texture toggles; shader effects (back faces hidden unless the shader
  is two sided, alpha test, blending)
- Measurements (Alt+M): width, depth and height lines of the whole model with its attached models, with the
  values next to the lines in game units, metric or imperial (1 game unit = 1 inch, the usual Quake 3 / ET scale)
- Two sided (no culling) and flat shading toggles; gamma and intensity sliders like the game's `r_gamma` and
  `r_intensity`
- Background: a color, a gradient (top and bottom colors) or a sky box of the game (the shaders' `skyParms`, sides
  `_rt` `_bk` `_lf` `_ft` `_up` `_dn` placed like the game's renderer), picked in the settings
- Ground grid that reaches the edges of the view and fades out, with the X / Y / Z axes
- Overlays: bounding box, hitboxes, tags (with names), skeleton, vertex normals
- Orientation gizmo in the corner (+X forward, +Y left, +Z up); clicking an axis looks from that side
- Orbit, pan and zoom camera, frame the model (F)
- Animation playback: play / pause, speed, frame range, interpolation between frames, looping, animation sequences
- Levels of detail: the MD3 levels and dynamic level of detail with the collapse maps
- Sharp text and 3D view on HiDPI / Retina screens
- Screenshots from the command line (with or without the user interface)

### Editing

- Model: name, transform (location, rotation, scale), export frame range
- Meshes (rename, material, remove, flip normals, reverse winding, flip texture U / V), tags (add, remove, rename,
  move to another bone), bones (rename, use as tag, torso weight), animation
  sequences (add, edit, remove)
- Edit menu: recalculate normals (smooth, smooth across texture seams, flat), Meshes submenu (merge meshes with the same material within the MD3
  limits, mirror X / Y / Z with the bones, tags and hitboxes, flip all normals, reverse all windings, flip texture V),
  bake the skin into the model (the skin's shaders become the
  mesh materials, also of the attached models, so an export keeps them), weld vertices (close cracks and gaps:
  vertices closer than a distance move to one place in every frame, optionally across meshes), calculate the level of detail collapse maps, optimize vertices, scale the bounding
  boxes, apply the transform, bake a pose into the bind pose, bake bone tags into tags, reorganize the armature to the
  ET player bones, check the armature
- **Gizmos**: select a tag or a hitbox and move (G) or rotate (R) it in the 3D view, scale (S) hitboxes; local or
  world axes, Ctrl snaps, Escape deselects. MD3 tags can be moved in every frame at once (keeping their animation)
- **Hitboxes**: boxes, spheres, cylinders and capsules on bones, with a list and an editor (name, bone, head
  modifier, shape, location, rotation, size), add / duplicate / delete
- Export to every writable format, one format at a time or "Export all" into a folder
- **glTF import of images and colors**: embedded images (in the GLB buffer or a data URI) are shown as the textures,
  tinted with the material's base color; a material with only a color gets a small image of that color (the tool has
  no material colors). The images stay with the model and are written again by the glTF export
- **glTF export** (`.glb` or `.gltf`): the model with every attached model on its tag or bone (player parts, an ET
  character's head and accessories), in meters (1 game unit = 1 inch) and glTF's Y-up. Skeletal models get a skin
  and one animation per sequence; MD3 / MDC frames become morph targets; tags become nodes. Materials take the skin's
  shaders, the textures are embedded (TGA converted to PNG) and follow the shader: two sided, alpha mask, blend.
  Models without a bind pose (MDM) use the frame where the weights agree best as the rest pose, and at most 4 weights
  per vertex are kept, so other frames can be a little off (the ET soldier: 1.5 cm at worst, 0.15 mm on average).
  `mt convert model out.glb` writes one model with its textures from the base folders

### Player models and attachments

- Opening an `animation.cfg` loads a three part player model: `lower.md3` as the model, `upper.md3` on its
  `tag_torso` and `head.md3` on the upper's `tag_head`, with their levels of detail
- Skins (`lower_<name>.skin`, ...) can be switched, legs and torso sequences are picked separately (death sequences
  play on both)
- Opening an ET character (`.char`) builds the player like the game draws it: the body mesh with every MDX file of
  its animation group (`.anim`, sequences from `#include`d `.aninc` files or inline), the body skin
  (`<mesh>_<skin>.skin`), the hud head on `tag_head` with its skin and head animations (`HD_IDLE1` ...), and the
  skin's accessories on their tags (`md3_back` on `tag_back`, belts on `tag_bright` / `tag_bleft` / `tag_ubelt`,
  `md3_weapon(2)` on `tag_weapon(2)`, `md3_hat` / `md3_hat2` / `md3_hat3` on the head's `tag_mouth`) with their
  optional `<model>_<skin>.skin`. Body and head sequences are picked in the Character section, and the undressed
  corpse (model and skin, no accessories) can be shown. The rank badge (`md3_rank`) is not drawn, as for an
  unranked player; the animation script is not read
- Any model can be attached to a tag or a bone of the model (or of another attached model) with the Attach button of
  the tag or bone; attached models follow it in every frame and have their own frames and sequences

### Application

- File picker for the base folders: the base folders are the root and the `.pk3` / `.pk3dir` contents look like
  normal folders; the source of every file is shown; fuzzy search over every file (`robot hd` finds
  `models/players/robot/head.md3`)
- Recent models, the last used folder, the window, the layout, the view options and the settings are kept between runs
- Settings: theme (Darker, Dark, Light), font size, vertical sync, case insensitive file names, scene background
  (color, gradient or sky box)
- Log panel with level filter and search
- The Hack font and Font Awesome icons are built into the program

### Keyboard

| Key                           | Action                                                         |
| ----------------------------- | -------------------------------------------------------------- |
| Ctrl+O                        | Open model (Cmd on macOS)                                      |
| Ctrl+,                        | Settings                                                       |
| Ctrl+Q                        | Quit                                                           |
| Space                         | Play / pause                                                   |
| F                             | Frame the model                                                |
| W / T / N / L                 | Wireframe / textures / interpolation / dynamic level of detail |
| Ctrl+B                        | Bounding box                                                   |
| Alt+O / Alt+T / Alt+S / Alt+N | Grid / tags / skeleton / normals                               |
| Alt+M                         | Measurements                                                   |
| G / R / S                     | Move / rotate / scale gizmo                                    |
| Escape                        | Deselect the tag or hitbox                                     |
| Shift+Escape                  | Show / hide the log                                            |

## Usage

On the first start the application asks for the base folder: the game's mod folder, like `etmain`. Models can then be
opened from the base folders (File > Open from base folders), or from anywhere on the disk (File > Open model).

Mouse in the 3D view: left drag orbits, right or middle drag pans, the wheel zooms.

### Command line options of the application

```
ModelTool [--base folder]... [--model path] [--animation path]... [--hitboxes file] [--frame n]
          [--skin name] [--attach tag=file]... [--sequence name]...
          [--sky name | --gradient r,g,b]
          [--view wireframe,notextures,alpha,noshaders,twosided,flat,measure,metric,imperial,nogrid,bounds,nohitboxes,tags,skeleton,normals,dynamiclod]
          [--export out.ext]... [--screenshot out.png [--ui]] [--size WxH] [--frames n]
```

For example a screenshot of an animated model, without opening a window:

```
ModelTool --base etmain --model models/players/temperate/body.mdm --animation animations/human/base/body.mdx \
          --frame 100 --view skeleton,tags --screenshot body.png
```

### Command line tool `mt`

```
mt [--base folder]... <command> [options]

  formats                        list the readable and writable formats
  info <model>                   meshes, bones, tags, hitboxes, animation sequences, bounds
                                 (animation.cfg: the player model's parts, skins and sequences;
                                 .char: the ET character's files, accessories and sequences)
  convert <model> <output>...    write the model, the format comes from the output extension
  list [folder]                  list the model files in the base folders and pk3 files

  --animation file, --hitboxes file, --frames first:last, --scale s, --normals smooth|seams|flat, --remove-mesh name, --merge-meshes, --mirror x|y|z,
  --skin file, --weld d, --weld-meshes,
  --optimize, --lod, --save-hitboxes file, --case-sensitive, --help, --version
```

```
mt --base etmain info models/mapobjects/tree/tree.md3
mt convert bob.md5mesh bob.md3 bob.mdm --animation bob.md5anim --frames 0:10 --scale 2
```

Exit codes: 0 ok, 1 failed, 2 invalid command line.

## Compile

Needs CMake 4.1 or newer and a C++17 compiler. The build works on Windows, macOS and Linux (OpenGL 4.1).

Fetch the submodules:

```
git submodule update --init
```

Build:

```
cmake -B build -S .
cmake --build build --config Release
```

- Tests: `ctest` in the build directory (GoogleTest, the fixtures are in `data/`)
- Test models: the MD3, MDC, MDM, MS3D, player, ET character and FBX cone / cube fixtures are generated by `tools/fixtures/generate_fixtures.cpp`
  (`cmake --build build --target generate_fixtures`, then `./build/generate_fixtures data`)
- Hitboxes for ET characters: `tools/hitboxes/fit_hitboxes.cpp` fits a small hitbox set to the body and head of one or
  more `.char` files (several give one set for all of them, e.g. every class of both teams), without the accessories, so that it holds the mesh in every frame of the animation group: a head box from the head
  bone up, capsules for the arms and legs, and the smallest of box / sphere / cylinder / capsule for the torso and feet
  (`cmake --build build --target fit_hitboxes`, then
  `./build/fit_hitboxes <base> characters/temperate/allied/medic.char [more.char...] medic.hitboxes [--single-pose [--sequence name | --frame n]] [--coverage percent] [--margin units] [--skip-coat]`).
  `--coverage` (default 100) is the percent of each part's vertex positions its hitbox must hold: the farthest ones are
  left out, giving smaller hitboxes that let some of the mesh out. `--margin` (default 0.05) is the space between the
  hitboxes and the vertices they hold; negative values fit inside the mesh. `--skip-coat` leaves out the coat (upper body
  mesh vertices on the leg bones or more than 2 units below the hip joints). It prints the volume of every hitbox and the
  total, and writes the total volume, settings and coverage as a comment at the end of the file
- Application: `build/ModelTool` (`build/ModelTool.app` on macOS); on some unix platforms
  `LC_CTYPE=en_US.UTF-8 build/ModelTool` might be needed
- Command line tool: `build/mt`
- Version: taken from the latest `v<major>.<minor>.<patch>` git tag (`git describe`): `1.2.3` on the tagged commit,
  `1.2.3-4-gabcdef0` after it, `0.0.0` without git or a tag. Releases are made by pushing a tag like `v1.2.3`
- Packages: `cpack -G ZIP` in the build directory (DragNDrop on macOS, WIX on Windows and TGZ on Linux too)
- `-DNATIVE_DIALOG=ON` uses the system's file dialogs (nativefiledialog)

## Libraries

| Library                                                           | Version                                                                             | Used for                                            | License                           | In the source tree                 |
| ----------------------------------------------------------------- | ----------------------------------------------------------------------------------- | --------------------------------------------------- | --------------------------------- | ---------------------------------- |
| [Dear ImGui](https://github.com/ocornut/imgui)                    | 1.91.3 (docking)                                                                    | User interface                                      | MIT                               | submodule `libs/imgui`             |
| [GLFW](https://github.com/glfw/glfw)                              | 3.4                                                                                 | Window, input, OpenGL context                       | zlib                              | submodule `libs/glfw`              |
| [glad](https://github.com/Dav1dde/glad)                           | 2.0.8 (generated, GL 4.1 core)                                                      | OpenGL loader                                       | WTFPL / CC0, Apache-2.0 (Khronos) | generated files `libs/glad`        |
| [GLM](https://github.com/g-truc/glm)                              | 0.9.9.8                                                                             | Math                                                | MIT                               | submodule `libs/glm`               |
| [stb](https://github.com/nothings/stb)                            | stb_image 2.30, stb_image_write 1.16                                                | Loading textures, writing screenshots               | MIT / public domain               | submodule `libs/stb`               |
| [spdlog](https://github.com/gabime/spdlog)                        | 1.14.1                                                                              | Logging                                             | MIT                               | submodule `libs/spdlog`            |
| [tinygltf](https://github.com/syoyo/tinygltf)                     | 3.0.1 (C11)                                                                         | glTF and GLB import                                 | MIT                               | submodule `libs/tinygltf`          |
| [ufbx](https://github.com/ufbx/ufbx)                              | 0.23.1                                                                              | FBX and OBJ import                                  | MIT / public domain               | vendored `libs/ufbx`               |
| [zlib](https://github.com/madler/zlib) + minizip                  | 1.3.2                                                                               | Reading `.pk3` (zip) files                          | zlib                              | submodule `libs/zlib`              |
| [imgui-filebrowser](https://github.com/jackeri/imgui-filebrowser) | fork of [AirGuanZ/imgui-filebrowser](https://github.com/AirGuanZ/imgui-filebrowser) | File dialogs for the disk                           | MIT                               | submodule `libs/imgui-filebrowser` |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)           | commit df1c301 (2025-12-03)                                                         | Move / rotate / scale gizmos                        | MIT                               | vendored `libs/imguizmo`           |
| [GoogleTest](https://github.com/google/googletest)                | 1.15.2                                                                              | Tests (not in the program)                          | BSD-3-Clause                      | submodule `libs/googletest`        |
| [nativefiledialog](https://github.com/mlabbe/nativefiledialog)    | 1.1.6                                                                               | System file dialogs, only with `-DNATIVE_DIALOG=ON` | zlib                              | submodule `libs/nativefiledialog`  |

Fonts and icons built into the application:

| Font                                                                                                   | Used for            | License             |
| ------------------------------------------------------------------------------------------------------ | ------------------- | ------------------- |
| [Hack](https://github.com/source-foundry/Hack)                                                         | User interface text | MIT, Bitstream Vera |
| [Font Awesome 4.7](https://fontawesome.com/v4/)                                                        | Icons               | SIL OFL 1.1         |
| [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders) (`src/app/icons_font_awesome.h`) | Icon names          | zlib                |

Other media or resources used with the application:

| File                                                       | Used for                         | License                       | Source                                                                                                            |
| ---------------------------------------------------------- | -------------------------------- | ----------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| `data/models/glft/pf.glb`                                  | Used for development and testing | CC-BY-4.0                     | [Low-Poly Panzerfaust 3](https://sketchfab.com/3d-models/low-poly-panzerfaust-3-409081e73ed742588d44403ed1419e96) |
| `data/models/iqm/mrfixit.iqm`                              | Used for development and testing | MIT                           | [IQM dev kit](https://github.com/lsalzman/iqm)                                                                    |
| `data/models/fbx/mrfixit.fbx`                              | Used for development and testing | MIT                           | [IQM dev kit](https://github.com/lsalzman/iqm)                                                                    |
| `data/models/md5/bob_lamp_update_export.{md5mesh,md5anim}` | Used for development and testing | Unlicensed (creator: der_ton) | [Multiple examples](https://www.3dgep.com/loading-and-animating-md5-models-with-opengl/)                          |
| `data/models/md3/tube.md3`, `data/models/mdc/tube.mdc`     | Used for development and testing | MIT (part of ModelTool)       | Generated by `tools/fixtures/generate_fixtures.cpp`                                                               |
| `data/models/mdm/mannequin.{mdm,mdx,aninc}`                | Used for development and testing | MIT (part of ModelTool)       | Generated by `tools/fixtures/generate_fixtures.cpp`                                                               |
| `data/models/players/robot/*`                              | Used for development and testing | MIT (part of ModelTool)       | Generated by `tools/fixtures/generate_fixtures.cpp`                                                               |
| `data/models/ms3d/test.ms3d`                               | Used for development and testing | MIT (part of ModelTool)       | Generated by `tools/fixtures/generate_fixtures.cpp`                                                               |
| `data/models/fbx/{cone,cube}.fbx`                         | Used for development and testing | MIT (part of ModelTool)       | Generated by `tools/fixtures/generate_fixtures.cpp`                                                               |
| `data/scripts/mannequin.shader` | Used for development and testing | MIT (part of ModelTool) | Generated by `tools/fixtures/generate_fixtures.cpp` |
| `data/characters/mannequin/*`, `data/animations/*`, `data/models/players/mannequin/*`, `data/models/mdm/mannequin_*.skin` | Used for development and testing | MIT (part of ModelTool) | Generated by `tools/fixtures/generate_fixtures.cpp` (ET test character) |

# License

ModelTool is licensed under the MIT License, see [LICENSE](./LICENSE.txt) for more information.
