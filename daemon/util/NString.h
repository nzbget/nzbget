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


#ifndef NSTRING_H
#define NSTRING_H

class NString
{
public:
	virtual size_t Capacity() const = 0;
	virtual bool Buffered() const = 0;
	virtual char* Data() const = 0;
	operator char*() const { return Data(); }
	char* operator*() const { return Data(); }
	size_t Length() const { char* buf = Data(); return buf ? strlen(Data()) : 0; }
	bool Empty() const { char* buf = Data(); return !buf || !*buf; }
	const char* Str() const { const char* buf = Data(); return buf ? buf : ""; }
	void Format(const char* format, ...);
	virtual void FormatV(const char* format, va_list ap);
};

class CString : public NString
{
private:
	char* m_data;

	CString(const CString& that) = delete;

public:
	CString() : m_data(NULL) {}

	CString(const char* str)
	{
		m_data = NULL;
		operator=(str);
	}

	CString(CString&& other)
	{
		m_data = other.m_data;
		other.m_data = NULL;
	}

	~CString()
	{
		free(m_data);
	}

	CString& operator=(const char* str)
	{
		char* newstr = str ? strdup(str) : NULL;
		free(m_data);
		m_data = newstr;
		return *this;
	}

	void Bind(char* str)
	{
		free(m_data);
		m_data = str;
	}

	char* Unbind()
	{
		char* olddata = m_data;
		m_data = NULL;
		return olddata;
	}

	virtual size_t Capacity() const { return Length(); }
	virtual bool Buffered() const { return false; }
	virtual char* Data() const { return m_data; }
	virtual void FormatV(const char* format, va_list ap);
};

template <size_t size>
class BString : public NString
{
private:
	char m_data[size + 1];

	BString(const BString& that) = delete;

public:
	BString()
	{
		m_data[0] = '\0';
		m_data[size] = '\0';
	}

	BString(const char* str)
	{
		m_data[0] = '\0';
		m_data[size] = '\0';
		operator=(str);
	}

	BString(int ignore, const char* format, ...)
	{
		m_data[0] = '\0';
		m_data[size] = '\0';

		va_list args;
		va_start(args, format);
		FormatV(format, args);
		va_end(args);
	}

	BString& operator=(const char* str)
	{
		strncpy(m_data, str, size);
		m_data[size - 1] = '\0';
		return *this;
	}

	virtual size_t Capacity() const { return size; }
	virtual bool Buffered() const { return true; }
	virtual char* Data() const { return const_cast<char*>(m_data); }
};

#if DEBUG
// helper declaration to identify incorrect calls to "free(CString)" at compile time
#ifdef WIN32
void _free_dbg(CString str, int ignore);
#else
void free(CString str);
#endif
#endif

#endif
