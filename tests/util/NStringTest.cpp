/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */
									 

#include "nzbget.h"

#include "catch.h"
					   
#include "NString.h"

TEST_CASE("CString", "[NString][Quick]")
{
	CString str;
	REQUIRE(str.Empty());
	REQUIRE(!str);
	str = "Hello, world";
	REQUIRE(!str.Empty());
	REQUIRE(str);
	REQUIRE(!strcmp(str, "Hello, world"));
	REQUIRE_FALSE(str.Buffered());

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str, "Hi, World!: 21"));

	char* tmp = strdup("Hello there");
	CString str2;
	str2.Bind(tmp);
	REQUIRE(!strcmp(str2, "Hello there"));
	REQUIRE(tmp == *str2);
	free(tmp);

	char* tmp2 = str2.Unbind();
	REQUIRE(tmp2 == tmp);
	REQUIRE(str2.Empty());
	REQUIRE(*str2 == NULL);

	CString str3("World 12345678901234567890");
	char buf[50];
	snprintf(buf, sizeof(buf), "Hi, %s%c: %i", *str3, '!', 21);
	REQUIRE(!strcmp(buf, "Hi, World 12345678901234567890!: 21"));

	CString str4;
	REQUIRE(*str4 == NULL);
	REQUIRE((char*)str4 == NULL);
	REQUIRE((const char*)str4 == NULL);
	REQUIRE(str4.Str() != NULL);
	REQUIRE(*str4.Str() == '\0');
}

TEST_CASE("BString", "[NString][Quick]")
{
	BString<100> str;
	REQUIRE(str.Empty());
	REQUIRE(str);
	str = "Hello, world";
	REQUIRE(!str.Empty());
	REQUIRE(str);
	REQUIRE(!strcmp(str, "Hello, world"));
	REQUIRE(str.Buffered());

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str, "Hi, World!: 21"));

	BString<5> str2;
	str2 = "Hello, world";
	REQUIRE(!strcmp(str2, "Hell"));

	str2.Format("Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str2, "Hi, "));

	BString<5> str3;
	strncpy(str3, "Hello, world", str3.Capacity());
	REQUIRE(!strncmp(str3, "Hello", 5));

	str3 = "World, Hello!";
	REQUIRE(!strcmp(str3, "Worl"));

	BString<30> str4("String 4 initialized");
	REQUIRE(!strcmp(str4, "String 4 initialized"));

	BString<20> str5(0, "Hi, %s%c: %i", "World", '!', 21);
	REQUIRE(!strcmp(str5, "Hi, World!: 21"));
}
