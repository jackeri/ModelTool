#include "gtest/gtest.h"
#include "script_stream.h"

using namespace mt;

TEST(ScriptStreamTest, test1)
{
	ScriptStream script(IO::MTPath("data").loadFile("test.script"));
	ASSERT_TRUE(script.valid());

	// Script should have tokens
	ASSERT_TRUE(script.hasNext());

	script.token();
	ASSERT_EQ(script.last(), "testBloc");
	ASSERT_TRUE(script.isBlockStart(true));

	ASSERT_EQ(script.token(), "name");
	ASSERT_EQ(script.token(), "value");

	ASSERT_EQ(script.token(), "innerBlock");
	ASSERT_TRUE(script.isBlockStart(true));
	ASSERT_EQ(script.token(), "escaped string with whitespace");
	ASSERT_TRUE(script.isBlockEnd(true));

	ASSERT_EQ(script.token(), "number");
	script.token();
	ASSERT_EQ(script.operator int(), 1);

	ASSERT_EQ(script.token(), "float");
	script.token();
	ASSERT_EQ(script.operator float(), 1.0f);

	ASSERT_EQ(script.token(), "boolean");
	script.token();
	ASSERT_TRUE(script.operator bool());

	ASSERT_EQ(script.token(), "vector");
	ASSERT_EQ(script.token(), "(");
	script.token();
	ASSERT_EQ(script.operator float(), 1.0f);
	script.token();
	ASSERT_EQ(script.operator float(), 1.0f);
	script.token();
	ASSERT_EQ(script.operator float(), 1.0f);
	ASSERT_EQ(script.token(), ")");

	// multiPunctuations
	ASSERT_EQ(script.token(), "value");
	ASSERT_EQ(script.token(), "+=");
	script.token();
	ASSERT_EQ(script.operator int(), 5);

	ASSERT_TRUE(script.isBlockEnd(true));

	// Script should have no more tokens
	ASSERT_FALSE(script.hasNext());
}
