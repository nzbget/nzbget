/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2015-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef NSTRING_H
#define NSTRING_H

/*
BString is a replacement for char-arrays allocated on stack.
It has no memory overhead, provides memory management and formatting functions.
 */
template <int size>
class BString
{
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
	int Format(const char* format, ...) PRINTF_SYNTAX(2);
	int FormatV(const char* format, va_list ap);

protected:
	char m_data[size];
};

/*
CString is a replacement for C-Style null-terminated strings.
It has no memory overhead, provides memory management and string handling functions.
 */
class CString
{
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
	int Format(const char* format, ...) PRINTF_SYNTAX(2);
	int FormatV(const char* format, va_list ap);
	int Find(const char* str, int pos = 0);
	void Replace(int pos, int len, const char* str, int strLen = 0);
	void Replace(const char* from, const char* to);
	void TrimRight();

protected:
	char* m_data = nullptr;
};

/*
Wide-character string.
 */
class WString
{
public:
	WString(wchar_t* wstr) : m_data(wcsdup(wstr)) {}
	WString(const char* utfstr);
	~WString() { free(m_data); }
	WString(WString&& other) noexcept { m_data = other.m_data; other.m_data = nullptr; }
	WString(WString& other) = delete;
	operator wchar_t*() const { return m_data; }
	wchar_t* operator*() const { return m_data; }
	int Length() { return wcslen(m_data); }

protected:
	wchar_t* m_data = nullptr;
};

/*
StringBuilder preallocates storage space and is best suitable for often "Append"s.
 */
class StringBuilder
{
public:
	~StringBuilder() { free(m_data); }
	operator const char*() const { return m_data ? m_data : ""; }
	explicit operator char*() { return m_data; }
	const char* operator*() const { return m_data; }
	int Length() const { return m_length; }
	void SetLength(int length) { m_length = length; }
	int Capacity() const { return m_capacity; }
	void Reserve(int capacity, bool exact = false);
	bool Empty() const { return m_length == 0; }
	void Clear();
	void Append(const char* str, int len = 0);
	void AppendFmt(const char* format, ...) PRINTF_SYNTAX(2);
	void AppendFmtV(const char* format, va_list ap);
	char* Unbind();

protected:
	char* m_data = nullptr;
	int m_length = 0;
	int m_capacity = 0;
};

/*
Plain char-buffer for I/O operations.
 */
class CharBuffer
{
public:
	CharBuffer() {}
	CharBuffer(int size) : m_size(size) { m_data = (char*)malloc(size); }
	CharBuffer(CharBuffer& other) : m_data(other.m_data), m_size(other.m_size) { other.m_data = nullptr; other.m_size = 0; }
	~CharBuffer() { free(m_data); }
	CharBuffer& operator=(CharBuffer&& other) = delete;
	int Size() { return m_size; }
	void Reserve(int size) { m_data = (char*)realloc(m_data, size); m_size = size; }
	void Clear() { free(m_data); m_data = nullptr; m_size = 0; }
	operator char*() const { return m_data; }
	char* operator*() const { return m_data; }

protected:
	char* m_data = nullptr;
	int m_size = 0;
};

#ifdef DEBUG
// helper declarations to identify incorrect calls to "free" at compile time
#ifdef WIN32
void _free_dbg(CString str, int ignore);
void _free_dbg(StringBuilder str, int ignore);
void _free_dbg(CharBuffer str, int ignore);
#else
void free(CString str);
void free(StringBuilder str);
void free(CharBuffer str);
#endif
#endif

#endif
