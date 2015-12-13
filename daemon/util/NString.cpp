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

void NString::Append(const char* str)
{
	size_t addLen = strlen(str);

	size_t curLen = Length();
	size_t capacity = Grow(curLen + addLen, true);
	size_t avail = capacity - curLen;

	char* buf = Data();
	strncpy(buf + curLen, str, avail);
	buf[capacity] = '\0';

	Resync(addLen <= (int)avail ? curLen + addLen : capacity);
}

void NString::AppendFmtV(const char* format, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);

	int addLen = vsnprintf(nullptr, 0, format, ap);

	size_t curLen = Length();
	size_t capacity = Grow(curLen + addLen, true);
	size_t avail = capacity - curLen;

	char* buf = Data();
	vsnprintf(buf + curLen, avail + 1, format, ap2);
	buf[capacity] = '\0';

	Resync(addLen <= (int)avail ? curLen + addLen : capacity);

	va_end(ap2);
}

void NString::Format(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	FormatV(format, args);
	va_end(args);
}

void NString::FormatV(const char* format, va_list ap)
{
	Clear();
	AppendFmtV(format, ap);
}
void NString::AppendFmt(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	AppendFmtV(format, args);
	va_end(args);
}


void CString::Clear()
{
	free(m_data);
	m_data = nullptr;
	m_capacity = 0;
	m_length = 0;
}

size_t CString::Capacity() const
{
	if (m_capacity == Unknown_Size)
	{
		m_capacity = Length();
	}
	return m_capacity;
}

size_t CString::Grow(size_t capacity, bool factor)
{
	size_t oldCapacity = Capacity();
	if (capacity > oldCapacity)
	{
		char* oldData = m_data;

		// we may grow more than requested
		size_t smartCapacity = factor ? (size_t)(oldCapacity * Grow_Factor) : 0;

		m_capacity = smartCapacity > capacity ? smartCapacity : capacity;

		m_data = (char*)realloc(m_data, m_capacity + 1);
		m_data[m_capacity] = '\0';
		if (!oldData)
		{
			m_data[0] = '\0';
		}
	}
	else if (!m_data)
	{
		m_capacity = Min_Grow_Capacity;
		m_data = (char*)malloc(m_capacity + 1);
		m_data[m_capacity] = '\0';
		m_data[0] = '\0';
	}
	return m_capacity;
}

CString& CString::operator=(const char* str)
{
	if (str)
	{
		m_capacity = strlen(str);
		char* newstr = (char*)malloc(m_capacity + 1);
		strncpy(newstr, str, m_capacity + 1);
		free(m_data);
		m_data = newstr;
		m_length = m_capacity;
	}
	else
	{
		free(m_data);
		m_data = nullptr;
		m_capacity = 0;
		m_length = 0;
	}
	return *this;
}

void CString::Bind(char* str)
{
	free(m_data);
	m_data = str;
	m_capacity = Unknown_Size;
	m_length = Unknown_Size;
}

char* CString::Unbind()
{
	char* olddata = m_data;
	m_data = nullptr;
	m_capacity = 0;
	m_length = 0;
	return olddata;
}


size_t CString::Length() const
{
	if (m_length == Unknown_Size)
	{
		m_length = m_data ? strlen(m_data) : 0;
	}
	return m_length;
}
