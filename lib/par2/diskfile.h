//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
//
//  par2cmdline is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  par2cmdline is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

#ifndef __DISKFILE_H__
#define __DISKFILE_H__

namespace Par2
{

// A disk file can be any type of file that par2cmdline needs
// to read or write data from or to.

class DiskFile
{
public:
  DiskFile(std::ostream& cerr);
  ~DiskFile(void);

  // Create a file and set its length
  bool Create(string filename, u64 filesize);

  // Write some data to the file
  bool Write(u64 offset, const void *buffer, size_t length);

  // Open the file
  bool Open(void);
  bool Open(string filename);
  bool Open(string filename, u64 filesize);

  // Check to see if the file is open
#ifdef WIN32
  bool IsOpen(void) const {return hFile != INVALID_HANDLE_VALUE;}
#else
  bool IsOpen(void) const {return file != 0;}
#endif

  // Read some data from the file
  bool Read(u64 offset, void *buffer, size_t length);

  // Close the file
  void Close(void);

  // Get the size of the file
  u64 FileSize(void) const {return filesize;}

  // Get the name of the file
  string FileName(void) const {return filename;}

  // Does the file exist
  bool Exists(void) const {return exists;}

  // Rename the file
  bool Rename(void); // Pick a filename automatically
  bool Rename(string filename);

  // Delete the file
  bool Delete(void);

public:
  static string GetCanonicalPathname(string filename);

  static void SplitFilename(string filename, string &path, string &name);
  static string TranslateFilename(string filename);

  static bool FileExists(string filename);
  static u64 GetFileSize(string filename);

  // Search the specified path for files which match the specified wildcard
  // and return their names in a list.
  static list<string>* FindFiles(string path, string wildcard);

protected:
  string filename;
  u64    filesize;

  // OS file handle
#ifdef WIN32
  HANDLE hFile;
#else
  FILE *file;
#endif

  // Current offset within the file
  u64    offset;

  // Does the file exist
  bool   exists;

protected:
#ifdef WIN32
  static string ErrorMessage(DWORD error);
#endif

  std::ostream& cerr;
};

// This class keeps track of which DiskFile objects exist
// and which file on disk they are associated with.
// It is used to avoid a file being processed twice.
class DiskFileMap
{
public:
  DiskFileMap(void);
  ~DiskFileMap(void);

  bool Insert(DiskFile *diskfile);
  void Remove(DiskFile *diskfile);
  DiskFile* Find(string filename) const;

protected:
  map<string, DiskFile*>    diskfilemap;             // Map from filename to DiskFile
};

} // end namespace Par2

#endif // __DISKFILE_H__
