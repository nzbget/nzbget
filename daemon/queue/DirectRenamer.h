/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DIRECTRENAMER_H
#define DIRECTRENAMER_H

#include "ArticleDownloader.h"

class RenameContentAnalyzer : public ArticleContentAnalyzer
{
public:
	virtual ~RenameContentAnalyzer();
	virtual void Reset();
	virtual void Append(const void* buffer, int len);
	void Finish(FileInfo* fileInfo, ArticleInfo* articleInfo);

private:
	// declared as void* to prevent inclusion if par2-modules into this header file
	void* m_md5Context = nullptr;
	int m_dataSize = 0;
};

#endif
