/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2009 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifndef SCANNER_H
#define SCANNER_H

class Scanner
{
private:
	bool				m_bRequestedNZBDirScan;
	int					m_iNZBDirInterval;
	bool				m_bNZBScript;
	bool				m_bSecondScan;
	int					m_iStepMSec;

	void				CheckIncomingNZBs(const char* szDirectory, const char* szCategory, bool bCheckTimestamp);
	void				AddFileToQueue(const char* szFilename, const char* szCategory);
	void				ProcessIncomingFile(const char* szDirectory, const char* szBaseFilename, const char* szFullFilename, const char* szCategory);

public:
						Scanner();
	void				SetStepInterval(int iStepMSec) { m_iStepMSec = iStepMSec; }
	void				ScanNZBDir();
	void				Check();
};

#endif
