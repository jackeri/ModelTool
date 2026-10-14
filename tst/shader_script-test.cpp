#include "gtest/gtest.h"
#include "filesystem.h"
#include "shader_script.h"

#include <string>

using namespace mt;
using namespace mt::model;

TEST(ShaderScriptTest, Key)
{
	// Like the game: no case, forward slashes, the extension of the last part removed
	EXPECT_EQ(shaderKey("Models\\Players\\Strap.TGA"), "models/players/strap");
	EXPECT_EQ(shaderKey("models/a.b/strap"), "models/a.b/strap");
	EXPECT_EQ(shaderKey("textures/x"), "textures/x");
}

TEST(ShaderScriptTest, Parse)
{
	const auto shaders = parseShaderScript(R"(
textures/test/plain{
	{ map textures/test/plain.tga }
}
textures/test/two
{
	cull TWOSIDED
	{
		map $whiteimage
	}
	{
		map "textures/test/two.jpg"
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}
textures/test/blend
{
	{
		map textures/test/blend.tga
		blendfunc blend
	}
	{
		map textures/test/ignored.tga
	}
}
textures/test/masked
{
	implicitMask -
}
textures/test/written
{
	{
		map textures/test/written.tga
		blendFunc GL_ONE GL_ONE
		depthWrite
	}
}
)",
										   "scripts/test.shader");
	ASSERT_EQ(shaders.size(), 5);
	EXPECT_EQ(shaders[0].name, "textures/test/plain");
	EXPECT_EQ(shaders[0].file, "scripts/test.shader");
	EXPECT_EQ(shaders[0].texture, "textures/test/plain.tga");
	EXPECT_EQ(shaders[0].cull, ShaderCull::Front);
	EXPECT_FALSE(shaders[0].blend);
	EXPECT_TRUE(shaders[0].depthWrite);

	// The white base stage is skipped, the image stage on it is drawn opaque on its own
	EXPECT_EQ(shaders[1].cull, ShaderCull::None);
	EXPECT_EQ(shaders[1].texture, "textures/test/two.jpg");
	EXPECT_EQ(shaders[1].stages, 2);
	EXPECT_FALSE(shaders[1].blend);

	// The first image stage counts, blended stages do not write the depth
	EXPECT_EQ(shaders[2].texture, "textures/test/blend.tga");
	EXPECT_TRUE(shaders[2].blend);
	EXPECT_EQ(shaders[2].blendSrc, BlendFactor::SrcAlpha);
	EXPECT_EQ(shaders[2].blendDst, BlendFactor::OneMinusSrcAlpha);
	EXPECT_FALSE(shaders[2].depthWrite);

	// implicitMask - : the shader name is the image, alpha tested
	EXPECT_EQ(shaders[3].texture, "textures/test/masked");
	EXPECT_EQ(shaders[3].alphaFunc, AlphaFunc::GE128);

	EXPECT_TRUE(shaders[4].blend);
	EXPECT_EQ(shaders[4].blendDst, BlendFactor::One);
	EXPECT_TRUE(shaders[4].depthWrite);

	// A broken shader ends the file, the ones before it are kept
	const auto broken = parseShaderScript("a/ok\n{\n\t{\n\t\tmap a/ok.tga\n\t}\n}\na/broken\n{\n\t{\n\t\tmap a/b.tga\n");
	ASSERT_EQ(broken.size(), 1);
	EXPECT_EQ(broken[0].name, "a/ok");
	EXPECT_TRUE(parseShaderScript("no_brace\nmap x\n").empty());
	EXPECT_TRUE(parseShaderScript("").empty());
}

TEST(ShaderScriptTest, Library)
{
	io::FileSystem files;
	ASSERT_TRUE(files.addPath("data"));

	ShaderLibrary shaders;
	EXPECT_EQ(shaders.load(files), 1);
	EXPECT_EQ(shaders.size(), 5);

	// The first definition in a file counts
	const ShaderDef *gear = shaders.find("Models/Players/Mannequin/Gear.tga");
	ASSERT_NE(gear, nullptr);
	EXPECT_EQ(gear->cull, ShaderCull::None);
	EXPECT_EQ(gear->texture, "models/players/mannequin/gear.tga");

	const ShaderDef *strap = shaders.find("models/players/mannequin/strap");
	ASSERT_NE(strap, nullptr);
	EXPECT_EQ(strap->alphaFunc, AlphaFunc::GE128);

	// animMap after a lightmap: the first frame, opaque
	const ShaderDef *glass = shaders.find("models/players/mannequin/glass");
	ASSERT_NE(glass, nullptr);
	EXPECT_EQ(glass->texture, "models/players/mannequin/medic.tga");
	ASSERT_EQ(glass->animFrames.size(), 2);
	EXPECT_FLOAT_EQ(glass->animFrequency, 4.0f);
	EXPECT_FALSE(glass->blend);
	EXPECT_EQ(glass->surfaceParms, std::vector<std::string>{"trans"});

	const ShaderDef *decal = shaders.find("models/players/mannequin/decal");
	ASSERT_NE(decal, nullptr);
	EXPECT_EQ(decal->cull, ShaderCull::Back);
	EXPECT_TRUE(decal->clamp);
	EXPECT_TRUE(decal->environment);
	EXPECT_TRUE(decal->noPicmip);
	EXPECT_EQ(decal->alphaFunc, AlphaFunc::GT0);
	EXPECT_TRUE(decal->blend);
	EXPECT_EQ(decal->blendSrc, BlendFactor::One);
	EXPECT_EQ(decal->surfaceParms.size(), 2);

	// A later script replaces the shaders of an earlier one
	shaders.add(parseShaderScript("models/players/mannequin/strap\n{\n\timplicitMap models/players/mannequin/naked.tga\n}\n"));
	EXPECT_EQ(shaders.find("models/players/mannequin/strap")->alphaFunc, AlphaFunc::None);
	EXPECT_EQ(shaders.size(), 5);
	EXPECT_EQ(shaders.find("models/players/mannequin/missing"), nullptr);
}

TEST(ShaderScriptTest, MaterialTexture)
{
	io::FileSystem files;
	ASSERT_TRUE(files.addPath("data"));
	ShaderLibrary shaders;
	shaders.load(files);

	// Through the shader, its editor image when it has no stages, else the image named like the material
	EXPECT_EQ(findMaterialTexture(files, shaders, "models/players/mannequin/strap", ""), "models/players/mannequin/gear.tga");
	EXPECT_EQ(findMaterialTexture(files, shaders, "models/players/mannequin/editor_only", ""), "models/players/mannequin/naked.tga");
	EXPECT_EQ(findMaterialTexture(files, shaders, "models/players/mannequin/helmet", ""), "models/players/mannequin/helmet.tga");
	EXPECT_EQ(findMaterialTexture(files, shaders, "models/players/mannequin/nothing", ""), "");

	// Without shaders only the image name is used
	ShaderLibrary none;
	EXPECT_EQ(findMaterialTexture(files, none, "models/players/mannequin/strap", ""), "");
}
