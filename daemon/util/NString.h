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

/*
 BString is a replacement for char-arrays allocated on stack.
 It has no memory overhead, provides memory management and formatting functions.
*/
template <int size>
class BString
{
protected:
	char m_data[size];
public:
	BString() { m_data[0] = '\0'; }
	explicit BString(const char* format, ...) PRINTF_SYNTAX(2);
	BString(BString& other) = delete;
	BString(const char* str) { Set(str); } // for initialization via assignment
	BString(BString&& other) noexcept { Set(other.m_data); } // never used but declaration is needed for initialization via assignment
	BString& operator=(const char* str) { Set(str); return *this; }
	int Length() const { return (int)strlen(m_data); }
	int Capacity() const { return size - 1; }
	bool Empty() const { return !*m_data; }
	void Clear() { m_data[0] = '\0'; }
	const char* Str() const { return m_data; }
	operator char*() const { return const_cast<char*>(m_data); }
	char* operator*() const { return const_cast<char*>(m_data); }
	void Set(const char* str, int len = 0);
	void Append(const char* str, int len = 0);
	void AppendFmt(const char* format, ...) PRINTF_SYNTAX(2);
	void AppendFmtV(const char* format, va_list ap);
	void Format(const char* format, ...) PRINTF_SYNTAX(2);
	void FormatV(const char* format, va_list ap);
};

/*
 CString is a replacement for C-Style null-terminated strings.
 It has no memory overhead, provides memory management and string handling functions.
*/
class CString
{
protected:
	char* m_data = nullptr;
public:
	CString() {}
	~CString() { free(m_data); }
	CString(const char* str, int len = 0) { Set(str, len); }
	CString(CString&& other) noexcept { m_data = other.m_data; other.m_data = nullptr; }
	CString(CString& other) = delete;
	CString& operator=(CString&& other) { free(m_data); m_data = other.m_data; other.m_data = nullptr; return *this; }
	CString& operator=(const char* str) { Set(str); return *this; }
	bool operator==(const CString& other);
	bool operator==(const char* other);
	static CString FormatStr(const char* format, ...);
	operator char*() const { return m_data; }
	char* operator*() const { return m_data; }
	const char* Str() const { return m_data ? m_data : ""; }
	int Length() const { return m_data ? (int)strlen(m_data) : 0; }
	bool Empty() const { return !m_data || !*m_data; }
	void Clear() { free(m_data); m_data = nullptr; }
	void Reserve(int capacity);
	void Bind(char* str);
	char* Unbind();
	void Set(const char* str, int len = 0);
	void Append(const char* str, int len = 0);
	void AppendFmt(const char* format, ...) PRINTF_SYNTAX(2);
	void AppendFmtV(const char* format, va_list ap);
	void Format(const char* format, ...) PRINTF_SYNTAX(2);
	void FormatV(const char* format, va_list ap);
	int Find(const char* str, int pos = 0);
	void Replace(int pos, int len, const char* str, int strLen = 0);
	void Replace(const char* from, const char* to);
	void TrimRight();
};

/*
 StringBuilder preallocates storage space and is best suitable for often "Append"s.
*/
class StringBuilder
{
protected:
	char* m_data = nullptr;
	int m_length = 0;
	int m_capacity = 0;
public:
	~StringBuilder() { free(m_data); }
	operator const char*() const { return m_data ? m_data : ""; }
	explicit operator char*() { return m_data; }
	const char* operator*() const { return m_data; }
	int Length() const { return m_length; }
	int Capacity() const { return m_capacity; }
	void Reserve(int capacity, bool exact = false);
	bool Empty() const { return m_length == 0; }
	void Clear();
	void Append(const char* str, int len = 0);
	void AppendFmt(const char* format, ...) PRINTF_SYNTAX(2);
	void AppendFmtV(const char* format, va_list ap);
	char* Unbind();
};

#ifdef DEBUG
#ifdef WIN32
// helper declaration to identify incorrect calls to "free(CString)" at compile time
void _free_dbg(CString str, int ignore);
void _free_dbg(StringBuilder str, int ignore);
#else
void free(CString str);
void free(StringBuilder str);
#endif
#endif

#endif
