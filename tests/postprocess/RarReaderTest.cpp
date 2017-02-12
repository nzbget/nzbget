/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "RarReader.h"
#include "FileSystem.h"
#include "TestUtil.h"

TEST_CASE("Rar-reader: rar3", "[Rar][RarReader][Slow][TestData]")
{
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part01.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part02.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3.part03.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 2);
	}
}

TEST_CASE("Rar-reader: rar5", "[Rar][RarReader][Slow][TestData]")
{
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5.part01.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5.part02.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5.part03.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 2);
	}
}

TEST_CASE("Rar-reader: rar3 old naming", "[Rar][RarReader][Slow][TestData]")
{
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3oldnam.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == false);
		REQUIRE(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3oldnam.r00").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == false);
		REQUIRE(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3oldnam.r01").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == false);
		REQUIRE(volume.GetVolumeNo() == 2);
	}
}

#ifndef DISABLE_TLS

TEST_CASE("Rar-reader: rar3 encrypted data", "[Rar][RarReader][Slow][TestData]")
{
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3encdata.part01.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3encdata.part02.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3encdata.part03.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 2);
	}
}

TEST_CASE("Rar-reader: rar5 encrypted data", "[Rar][RarReader][Slow][TestData]")
{
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5encdata.part01.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5encdata.part02.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5encdata.part03.rar").c_str());
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 2);
	}
}

TEST_CASE("Rar-reader: rar3 encrypted names", "[Rar][RarReader][Slow][TestData]")
{
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3encnam.part01.rar").c_str());
		REQUIRE(volume.Read() == false);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetEncrypted() == true);
	}

	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3encnam.part01.rar").c_str());
		volume.SetPassword("123");
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3encnam.part02.rar").c_str());
		volume.SetPassword("123");
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile3encnam.part03.rar").c_str());
		volume.SetPassword("123");
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 3);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 2);
	}
}

TEST_CASE("Rar-reader: rar5 encrypted names", "[Rar][RarReader][Slow][TestData]")
{
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5encnam.part01.rar").c_str());
		volume.SetPassword("123");
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5encnam.part02.rar").c_str());
		volume.SetPassword("123");
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5encnam.part03.rar").c_str());
		volume.SetPassword("123");
		REQUIRE(volume.Read() == true);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetMultiVolume() == true);
		REQUIRE(volume.GetNewNaming() == true);
		REQUIRE(volume.GetVolumeNo() == 2);
	}

	{
		RarVolume volume((TestUtil::TestDataDir() + "/rarrenamer/testfile5encnam.part01.rar").c_str());
		REQUIRE(volume.Read() == false);
		REQUIRE(volume.GetVersion() == 5);
		REQUIRE(volume.GetEncrypted() == true);
	}
}

#endif
