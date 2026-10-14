#include "gtest/gtest.h"
#include "filesystem.h"
#include "image.h"
#include "model_gltf_write.h"
#include "morph_model.h"
#include "skin.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

using namespace mt;
using namespace mt::model;

namespace {
	std::string base64(const std::vector<byte> &data)
	{
		static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		for (size_t i = 0; i < data.size(); i += 3)
		{
			uint32_t triple = uint32_t(data[i]) << 16 | (i + 1 < data.size() ? uint32_t(data[i + 1]) << 8 : 0)
							  | (i + 2 < data.size() ? uint32_t(data[i + 2]) : 0);
			out += alphabet[(triple >> 18) & 63];
			out += alphabet[(triple >> 12) & 63];
			out += i + 1 < data.size() ? alphabet[(triple >> 6) & 63] : '=';
			out += i + 2 < data.size() ? alphabet[triple & 63] : '=';
		}
		return out;
	}

	std::unique_ptr<Model> loadFile(const std::string &name, const std::string &text)
	{
		std::filesystem::create_directories("tmp/gltf_images");
		const std::string path = "tmp/gltf_images/" + name;
		std::ofstream(path, std::ios::binary) << text;
		mt::io::FileSystem disk;
		return std::unique_ptr<Model>(mt::model::io::loadModel(disk.loadFile(std::filesystem::absolute(path).string())));
	}

	mt::io::Image decoded(const std::vector<byte> &data)
	{
		mt::io::Image image;
		EXPECT_TRUE(mt::io::decodeImage(data.data(), data.size(), image));
		return image;
	}

	/*! Two triangles with the materials 0 and 1, the second material has an embedded 2 x 1 image */
	std::string twoTriangles(const std::string &materials)
	{
		std::vector<byte> buffer(36 + 8);
		const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
		const uint16_t indices[3] = {0, 1, 2};
		std::memcpy(buffer.data(), positions, 36);
		std::memcpy(buffer.data() + 36, indices, 6);

		mt::io::Image image;
		image.width = 2;
		image.height = 1;
		image.pixels = {200, 100, 50, 255, 100, 100, 100, 255};
		const std::string png = base64(mt::io::encodePNG(image));

		return R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,1]}],
"nodes":[{"mesh":0},{"mesh":1,"translation":[2,0,0]}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]},
          {"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":1}]}],
"materials":)" + materials
			   + R"(,
"textures":[{"source":0}],
"images":[{"uri":"data:image/png;base64,)"
			   + png + R"("}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,)"
			   + base64(buffer) + R"("}]})";
	}
}

TEST(GltfImagesTest, ColorsAndTints)
{
	// A color without a texture, and the embedded image tinted with half red
	auto loaded = loadFile("colors.gltf", twoTriangles(R"([{"name":"paint","pbrMetallicRoughness":{"baseColorFactor":[1,0.5,0,1]}},
		{"pbrMetallicRoughness":{"baseColorFactor":[0.5,1,1,1],"baseColorTexture":{"index":0}}}])"));
	auto &model = dynamic_cast<morph::MorphModel &>(*loaded);
	ASSERT_EQ(model.meshes.size(), 2);
	EXPECT_EQ(model.meshes[0].materialName, "paint");
	EXPECT_EQ(model.meshes[1].materialName, "image 0"); // no names: named after the image
	ASSERT_EQ(model.images.size(), 2);

	const auto color = decoded(model.images.at("paint"));
	EXPECT_EQ(color.width, 4);
	EXPECT_EQ(color.pixels[0], 255);
	EXPECT_EQ(color.pixels[1], 128);
	EXPECT_EQ(color.pixels[2], 0);

	const auto tinted = decoded(model.images.at("image 0"));
	ASSERT_EQ(tinted.width, 2);
	EXPECT_EQ(tinted.pixels[0], 100); // 200 * 0.5
	EXPECT_EQ(tinted.pixels[1], 100);
	EXPECT_EQ(tinted.pixels[4], 50);

	// Without a name the color names the material
	auto unnamed = loadFile("unnamed.gltf", twoTriangles(R"([{"pbrMetallicRoughness":{"baseColorFactor":[0,0,1,1]}},
		{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}])"));
	EXPECT_EQ(dynamic_cast<morph::MorphModel &>(*unnamed).meshes[0].materialName, "color #0000ff");
	// Not tinted: the image as it was in the file
	EXPECT_EQ(decoded(unnamed->images.at("image 0")).pixels[0], 200);
	std::filesystem::remove_all("tmp/gltf_images");
}

TEST(GltfImagesTest, ExportedTexturesComeBack)
{
	mt::io::FileSystem files;
	ASSERT_TRUE(files.addPath("data"));
	std::unique_ptr<Model> lower(mt::model::io::loadModel(files.loadFile("models/players/robot/lower.md3")));
	const Skin skin = loadSkin(files.loadFile("models/players/robot/lower_red.skin"));

	GltfPart part{lower.get(), "lower", "models/players/robot/lower.md3", -1, "", &skin};
	GltfOptions options;
	options.files = &files;
	const auto data = encodeGLTF({part}, options);
	std::unique_ptr<Model> loaded;
	{
		std::filesystem::create_directories("tmp/gltf_images");
		std::ofstream("tmp/gltf_images/lower.glb", std::ios::binary)
			.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
		mt::io::FileSystem disk;
		loaded.reset(mt::model::io::loadModel(disk.loadFile(std::filesystem::absolute("tmp/gltf_images/lower.glb").string())));
	}
	ASSERT_TRUE(loaded);

	// The skin's texture comes back as an image of the model, with the pixels of the original
	const auto &mesh = dynamic_cast<morph::MorphModel &>(*loaded).meshes.at(0);
	ASSERT_EQ(loaded->images.count(mesh.materialName), 1);
	auto original = files.loadFile("models/players/robot/red.tga");
	mt::io::Image expected;
	ASSERT_TRUE(mt::io::decodeImage(original->data->data(), original->len, expected));
	const auto image = decoded(loaded->images.at(mesh.materialName));
	EXPECT_EQ(image.width, expected.width);
	EXPECT_EQ(image.pixels, expected.pixels);

	// Exported again without files, the image travels with the model
	GltfPart again{loaded.get(), "again", "", -1, "", nullptr};
	const auto second = encodeGLTF({again}, GltfOptions{});
	const std::string json(second.begin() + 20, second.begin() + 20 + (second[12] | second[13] << 8 | second[14] << 16));
	EXPECT_NE(json.find("\"mimeType\":\"image/png\""), std::string::npos);
	std::filesystem::remove_all("tmp/gltf_images");
}
