/**************************************************************************/
/*  test_hl2_keyvalues.h                                                  */
/**************************************************************************/

#pragma once

#include "../hl2_keyvalues.h"

#include "tests/test_macros.h"

namespace TestHL2KeyValues {

TEST_CASE("[HL2Formats] Parse nested keyvalues") {
	HL2KeyValues parser;
	Dictionary parsed = parser.parse_text(R"("Root"
{
	"Name" "gordon"
	"Nested"
	{
		"Health" "100"
	}
}
)");

	CHECK(parser.get_last_error_message().is_empty());
	CHECK(parsed.has("Root"));
	Dictionary root = parsed["Root"];
	CHECK(root["Name"] == "gordon");
	Dictionary nested = root["Nested"];
	CHECK(nested["Health"] == "100");
}

TEST_CASE("[HL2Formats] Parse duplicate keys as ordered arrays") {
	HL2KeyValues parser;
	Dictionary parsed = parser.parse_text(R"(
"Entity"
{
	"OnTrigger" "door,Open"
	"OnTrigger" "light,TurnOn"
}
)");

	CHECK(parser.get_last_error_message().is_empty());
	Dictionary entity = parsed["Entity"];
	REQUIRE(entity.has("OnTrigger"));
	Array outputs = entity["OnTrigger"];
	REQUIRE(outputs.size() == 2);
	CHECK(outputs[0] == "door,Open");
	CHECK(outputs[1] == "light,TurnOn");
}

TEST_CASE("[HL2Formats] Parse comments and unquoted tokens") {
	HL2KeyValues parser;
	Dictionary parsed = parser.parse_text(R"(
// Comment line
Root
{
	ClassName npc_citizen // end-of-line comment
	/* block
	   comment */
	Squad alpha
}
)");

	CHECK(parser.get_last_error_message().is_empty());
	Dictionary root = parsed["Root"];
	CHECK(root["ClassName"] == "npc_citizen");
	CHECK(root["Squad"] == "alpha");
}

TEST_CASE("[HL2Formats] Invalid keyvalues reports line and column") {
	HL2KeyValues parser;
	Dictionary parsed = parser.parse_text(R"(
"Root"
{
	"Name" "alyx"
)");

	CHECK(parsed.is_empty());
	CHECK(!parser.get_last_error_message().is_empty());
	CHECK(parser.get_last_error_line() > 0);
	CHECK(parser.get_last_error_column() > 0);
}

} // namespace TestHL2KeyValues
