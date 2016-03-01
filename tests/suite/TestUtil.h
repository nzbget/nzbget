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


#ifndef TESTUTIL_H
#define TESTUTIL_H

class TestUtil
{
public:
	static void Init(const char* argv0);
	static void Final();
	static const std::string TestDataDir();
	static const std::string WorkingDir();
	static void PrepareWorkingDir(const std::string templateDir);
	static void CleanupWorkingDir();
	static void DisableCout();
	static void EnableCout();
	static void CopyAllFiles(const std::string destDir, const std::string srcDir);

private:
	static bool m_usedWorkingDir;
};

#endif
