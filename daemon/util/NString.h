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

/* Abstract base string class.
 */
class NString
{
protected:
	virtual size_t Grow(size_t capacity, bool factor) = 0;
	virtual void Resync(size_t newLength = Unknown_Size) {}

public:
	const static size_t Unknown_Size = (size_t)-1;

	virtual size_t Capacity() const = 0;
	size_t Reserve(size_t capacity) { return Grow(capacity, false); }
	virtual char* Data() const = 0;
	virtual void Clear() = 0;
	virtual size_t Length() const { char* buf = Data(); return buf ? strlen(Data()) : 0; }
	bool Empty() const { char* buf = Data(); return !buf || !*buf; }
	const char* Str() const { const char* buf = Data(); return buf ? buf : ""; }
	void Append(const char* str);
	void AppendFmt(const char* format, ...);
	void AppendFmtV(const char* format, va_list ap);
	void Format(const char* format, ...);
	void FormatV(const char* format, va_list ap);
};

/* Dynamic string class, whose content may grow.
 */
class CString : public NString
{
private:
	char* m_data;
	mutable size_t m_capacity;
	mutable size_t m_length;

	CString(const CString& that) = delete;

	virtual size_t Grow(size_t capacity, bool factor) override;

public:
	const static size_t Min_Grow_Capacity = 15;
	constexpr static double Grow_Factor = 1.5;

	CString() : m_data(nullptr), m_capacity(0), m_length(0) {}

	CString(const char* str) : m_data(nullptr), m_capacity(0), m_length(0)
	{
		operator=(str);
	}

	CString(CString&& other) : m_data(other.m_data), m_capacity(other.m_capacity), m_length(other.m_length)
	{
		other.m_data = nullptr;
		other.m_capacity = 0;
		other.m_length = 0;
	}

	~CString()
	{
		free(m_data);
	}

	virtual void Clear() override;
	virtual size_t Length() const override;
	virtual void Resync(size_t newLength = Unknown_Size) override { m_length = newLength; }
	bool Empty() const { return !m_data || !*m_data; }
	CString& operator=(const char* str);
	virtual size_t Capacity() const override;
	virtual char* Data() const override{ return m_data; }
	operator const char*() const { return m_data; }
	explicit operator char*() const { return m_data; }
	const char* operator*() const { return m_data; }
	void Bind(char* str);
	char* Unbind();
};

/* String class with statically allocated buffer of specified length.
   Best suitable as replacement for char-arrays allocated on stack.
 */
template <size_t size>
class BString : public NString
{
private:
	char m_data[size + 1];

	BString(const BString& that) = delete;

protected:
	virtual size_t Grow(size_t capacity, bool factor) override { return size; }

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

	virtual void Clear() override { m_data[0] = '\0'; }
	virtual size_t Capacity() const override { return size; }
	virtual char* Data() const override { return const_cast<char*>(m_data); }
	operator char*() const { return const_cast<char*>(m_data); }
	char* operator*() const { return m_data; }
};

#ifdef DEBUG
#ifdef WIN32
// helper declaration to identify incorrect calls to "free(CString)" at compile time
void _free_dbg(CString str, int ignore);
#else
void free(CString str);
#endif
#endif

#endif
