/*
 *  This file if part of nzbget
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
#include "NString.h"

void NString::Format(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	FormatV(format, args);
	va_end(args);
}

void NString::FormatV(const char* format, va_list ap)
{
	vsnprintf(Data(), Capacity(), format, ap);
}

void CString::FormatV(const char* format, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);

	int m = vsnprintf(m_data, 0, format, ap);
#ifdef WIN32
	if (m == -1)
	{
		m = _vscprintf(format, ap);
	}
#endif

	char* olddata = m_data;
	m_data = (char*)malloc(m + 1);
	vsnprintf(m_data, m + 1, format, ap2);
	free(olddata);

	va_end(ap2);
}
