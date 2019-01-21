/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *
 */


#ifndef SCANNER_H
#define SCANNER_H

#include "NString.h"
#include "DownloadInfo.h"
#include "Thread.h"
#include "Service.h"

class Scanner : public Service
{
public:
	enum EAddStatus
	{
		asSkipped,
		asSuccess,
		asFailed
	};

	void InitOptions();
	void ScanNzbDir(bool syncMode);
	EAddStatus AddExternalFile(const char* nzbName, const char* category, int priority,
		const char* dupeKey, int dupeScore, EDupeMode dupeMode,
		NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
		const char* fileName, const char* buffer, int bufSize, int* nzbId);
	void InitPPParameters(const char* category, NzbParameterList* parameters, bool reset);

protected:
	virtual int ServiceInterval();
	virtual void ServiceWork();

private:
	class FileData
	{
	public:
		FileData(const char* filename, int64 size, time_t lastChange) :
			m_filename(filename), m_size(size), m_lastChange(lastChange) {}
		const char* GetFilename() { return m_filename; }
		int64 GetSize() { return m_size; }
		void SetSize(int64 size) { m_size = size; }
		time_t GetLastChange() { return m_lastChange; }
		void SetLastChange(time_t lastChange) { m_lastChange = lastChange; }
	private:
		CString m_filename;
		int64 m_size;
		time_t m_lastChange;
	};

	typedef std::deque<FileData> FileList;

	class QueueData
	{
	public:
		QueueData(const char* filename, const char* nzbName, const char* category,
			int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
			NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
			EAddStatus* addStatus, int* nzbId);
		const char* GetFilename() { return m_filename; }
		const char* GetNzbName() { return m_nzbName; }
		const char* GetCategory() { return m_category; }
		int GetPriority() { return m_priority; }
		const char* GetDupeKey() { return m_dupeKey; }
		int GetDupeScore() { return m_dupeScore; }
		EDupeMode GetDupeMode() { return m_dupeMode; }
		NzbParameterList* GetParameters() { return &m_parameters; }
		bool GetAddTop() { return m_addTop; }
		bool GetAddPaused() { return m_addPaused; }
		NzbInfo* GetUrlInfo() { return m_urlInfo; }
		void SetAddStatus(EAddStatus addStatus);
		void SetNzbId(int nzbId);
	private:
		CString m_filename;
		CString m_nzbName;
		CString m_category;
		int m_priority;
		CString m_dupeKey;
		int m_dupeScore;
		EDupeMode m_dupeMode;
		NzbParameterList m_parameters;
		bool m_addTop;
		bool m_addPaused;
		NzbInfo* m_urlInfo;
		EAddStatus* m_addStatus;
		int* m_nzbId;
	};

	typedef std::deque<QueueData> QueueList;

	bool m_requestedNzbDirScan = false;
	int m_nzbDirInterval = 0;
	bool m_scanScript = false;
	int m_pass = 0;
	FileList m_fileList;
	QueueList m_queueList;
	bool m_scanning = false;
	Mutex m_scanMutex;
	static int m_idGen;

	void CheckIncomingNzbs(const char* directory, const char* category, bool checkStat);
	bool AddFileToQueue(const char* filename, const char* nzbName, const char* category,
		int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
		NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo, int* nzbId);
	void ProcessIncomingFile(const char* directory, const char* baseFilename,
		const char* fullFilename, const char* category);
	bool CanProcessFile(const char* fullFilename, bool checkStat);
	void DropOldFiles();
};

extern Scanner* g_Scanner;

#endif
