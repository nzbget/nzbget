/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
									 

#include "nzbget.h"

#include "catch.h"
					   
#include "NString.h"


TEST_CASE("BString", "[NString][Quick]")
{
	BString<100> str;
	REQUIRE(sizeof(str) == sizeof(char[100]));
	REQUIRE(str.Empty());
	REQUIRE(str);
	str = "Hello, world";
	REQUIRE(!str.Empty());
	REQUIRE(str);
	REQUIRE(!strcmp(str, "Hello, world"));

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str, "Hi, World!: 21"));

	BString<20> str2;
	str2 = "Hello, world 01234567890123456789";
	REQUIRE(!strcmp(str2, "Hello, world 012345"));

	str2.Format("0123456789 Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str2, "0123456789 Hi, Worl"));

	BString<20> str3;
	memcpy(str3, "Hello, 0123456789 world", str3.Capacity());
	str3[str3.Capacity()] = '\0';
	REQUIRE(!strcmp(str3, "Hello, 0123456789 w"));

	str3 = "String 3 Test World, Hello!";
	REQUIRE(!strcmp(str3, "String 3 Test World"));

	BString<100> str4;
	str4 = "String 4 initialized";
	REQUIRE(!strcmp(str4, "String 4 initialized"));

	BString<20> str5("Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str5, "Hi, World!: 21"));

	BString<20> str6;
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	str6.Append("0123456789", 5);
	REQUIRE(!strcmp(str6, "Hello, WorldString5"));

	BString<20> str7;
	str7.Append("0123456789", 5);
	REQUIRE(!strcmp(str7, "01234"));

	BString<20> str8;
	str8.Append("0123456789", 5);
	str8.AppendFmt("%i:%i", 87, 65);
	REQUIRE(!strcmp(str8, "0123487:65"));

	const char* txt = "String 9 initialized";
	BString<100> str9 = txt;
	REQUIRE(!strcmp(str9, "String 9 initialized"));
}

TEST_CASE("CString", "[NString][Quick]")
{
	CString str;
	REQUIRE(sizeof(str) == sizeof(char*));
	REQUIRE(str.Empty());
	REQUIRE(!str);
	str = "Hello, world";
	REQUIRE(!str.Empty());
	REQUIRE(str);
	REQUIRE(!strcmp(str, "Hello, world"));

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str, "Hi, World!: 21"));

	char* tmp = strdup("Hello there");
	CString str2;
	str2.Bind(tmp);
	const char* tmp3 = *str2;
	REQUIRE(tmp == tmp3);
	REQUIRE(!strcmp(str2, "Hello there"));
	REQUIRE(tmp == *str2);
	free(tmp);

	char* tmp2 = str2.Unbind();
	REQUIRE(tmp2 == tmp);
	REQUIRE(str2.Empty());
	REQUIRE(*str2 == nullptr);

	CString str3("World 12345678901234567890");
	char buf[50];
	snprintf(buf, sizeof(buf), "Hi, %s%c: %i", *str3, '!', 21);
	REQUIRE(!strcmp(buf, "Hi, World 12345678901234567890!: 21"));

	CString str4;
	REQUIRE(*str4 == nullptr);
	REQUIRE((char*)str4 == nullptr);
	REQUIRE((const char*)str4 == nullptr);
	REQUIRE(str4.Str() != nullptr);
	REQUIRE(*str4.Str() == '\0');

	CString str6;
	str6.Append("");
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	REQUIRE(!strcmp(str6, "Hello, WorldString5String567"));

	str6.Clear();
	str6.Append("0123456789", 5);
	str6.AppendFmt("%i:%i", 87, 65);
	REQUIRE(!strcmp(str6, "0123487:65"));

	std::vector<CString> vec1;
	vec1.push_back("Hello, there");
	CString& str7 = vec1.back();
	REQUIRE(!strcmp(str7, "Hello, there"));

	REQUIRE(!strcmp(CString::FormatStr("Many %s ago", "days"), "Many days ago"));

	CString str8("Hello, World");
	str8.Replace(1, 4, "i");
	REQUIRE(!strcmp(str8, "Hi, World"));
	str8.Replace(4, 5, "everybody");
	REQUIRE(!strcmp(str8, "Hi, everybody"));
	str8.Replace(4, 5, "nome", 2);
	REQUIRE(!strcmp(str8, "Hi, nobody"));

	CString str9 = " Long string with spaces \t\r\n ";
	str9.TrimRight();
	REQUIRE(!strcmp(str9, " Long string with spaces"));
	int pos = str9.Find("with");
	REQUIRE(pos == 13);
	str9.Replace("string", "with");
	REQUIRE(!strcmp(str9, " Long with with spaces"));
	str9.Replace("with", "without");
	REQUIRE(!strcmp(str9, " Long without without spaces"));
	str9.Replace("without", "");
	REQUIRE(!strcmp(str9, " Long   spaces"));

	str8 = "test string";
	str9 = "test string";
	REQUIRE(str8 == str9);
	bool eq = str8 == "test string";
	REQUIRE(eq);
}

TEST_CASE("StringBuilder", "[NString][Quick]")
{
	StringBuilder str6;
	str6.Append("");
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	REQUIRE(!strcmp(str6, "Hello, WorldString5String567"));
}

TEST_CASE("CharBuffer", "[NString][Quick]")
{
	CharBuffer buf(1024);
	REQUIRE(buf.Size() == 1024);
	buf.Reserve(2048);
	REQUIRE(buf.Size() == 2048);
	buf.Clear();
	REQUIRE(buf.Size() == 0);
}
