#include "gtest/gtest.h"
#include "ScriptStream.h"

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
	ASSERT_EQ((int) script, 1);

	ASSERT_EQ(script.token(), "float");
	script.token();
	ASSERT_EQ((float) script, 1.0f);

	ASSERT_EQ(script.token(), "boolean");
	script.token();
	ASSERT_TRUE((bool) script);
	ASSERT_TRUE(script.isBlockEnd(true));

	// Script should have no more tokens
	ASSERT_FALSE(script.hasNext());
}