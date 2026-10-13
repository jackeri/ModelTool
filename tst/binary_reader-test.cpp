#include "gtest/gtest.h"
#include "binary_reader.h"
#include "mt_path.h"

using namespace mt;
using namespace mt::io;

namespace {
	struct TestHeader {
		int ident;
		int version;
		char name[8];
	};

	std::vector<byte> testData()
	{
		std::vector<byte> data(sizeof(TestHeader) + sizeof(float) * 2);

		TestHeader header{0x33504449, 15, "head"};
		float values[2] = {1.5f, -2.0f};

		std::memcpy(data.data(), &header, sizeof(header));
		std::memcpy(data.data() + sizeof(header), values, sizeof(values));
		return data;
	}
}

TEST(BinaryReaderTest, readsStructsAndArrays)
{
	auto data = testData();
	BinaryReader reader(data.data(), data.size());

	auto header = reader.read<TestHeader>();
	ASSERT_EQ(header.ident, 0x33504449);
	ASSERT_EQ(header.version, 15);
	ASSERT_EQ(BinaryReader::toString(header.name), "head");

	auto values = reader.readArray<float>(2);
	ASSERT_FLOAT_EQ(values[0], 1.5f);
	ASSERT_FLOAT_EQ(values[1], -2.0f);
	ASSERT_EQ(reader.remaining(), 0);

	ASSERT_EQ(reader.readAt<int>(4), 15);
	ASSERT_EQ(reader.tell(), data.size());
}

TEST(BinaryReaderTest, readsFixedStrings)
{
	auto data = testData();
	BinaryReader reader(data.data(), data.size());

	reader.seek(8);
	ASSERT_EQ(reader.readString(8), "head");
	ASSERT_EQ(reader.tell(), 16);
}

TEST(BinaryReaderTest, throwsOnOutOfBounds)
{
	auto data = testData();
	BinaryReader reader(data.data(), data.size());

	ASSERT_THROW(reader.seek(-1), mt_exception);
	ASSERT_THROW(reader.seek(int64_t(data.size()) + 1), mt_exception);
	ASSERT_NO_THROW(reader.seek(int64_t(data.size())));
	ASSERT_THROW(reader.read<int>(), mt_exception);

	reader.seek(0);
	ASSERT_THROW(reader.readArray<float>(-1), mt_exception);
	ASSERT_THROW(reader.readArray<float>(1000), mt_exception);
	// count * sizeof would overflow
	ASSERT_THROW(reader.readArray<float>(INT64_MAX / 2), mt_exception);
	ASSERT_THROW((void)reader.readAt<int>(int64_t(data.size()) - 2), mt_exception);
	ASSERT_THROW(reader.skip(int64_t(data.size()) + 1), mt_exception);

	// a failed read does not move the offset
	ASSERT_EQ(reader.tell(), 0);
}

TEST(BinaryReaderTest, subReaders)
{
	auto data = testData();
	BinaryReader reader(data.data(), data.size());

	BinaryReader floats = reader.from(sizeof(TestHeader));
	ASSERT_EQ(floats.size(), sizeof(float) * 2);
	ASSERT_FLOAT_EQ(floats.read<float>(), 1.5f);

	BinaryReader version = reader.sub(4, 4);
	ASSERT_EQ(version.read<int>(), 15);
	ASSERT_THROW(version.read<int>(), mt_exception);

	ASSERT_THROW((void)reader.sub(4, int64_t(data.size())), mt_exception);
}

TEST(BinaryReaderTest, readsLoadedFiles)
{
	MTPath path("data");
	auto file = path.loadFile("models/md3/tube.md3");
	ASSERT_TRUE(file);

	// The terminating null byte added by the loader is not part of the file
	BinaryReader reader(file);
	ASSERT_EQ(reader.size(), 7092);
	ASSERT_EQ(reader.read<int>(), ('3' << 24) + ('P' << 16) + ('D' << 8) + 'I'); // "IDP3"
}
