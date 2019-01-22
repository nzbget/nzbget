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
 */


#include "nzbget.h"

#ifndef DISABLE_PARCHECK

#include "par2cmdline.h"
#include "par2repairer.h"

#include "ParChecker.h"
#include "ParParser.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"
#include "FileSystem.h"

const char* Par2CmdLineErrStr[] = { "OK",
	"data files are damaged and there is enough recovery data available to repair them",
	"data files are damaged and there is insufficient recovery data available to be able to repair them",
	"there was something wrong with the command line arguments",
	"the PAR2 files did not contain sufficient information about the data files to be able to verify them",
	"repair completed but the data files still appear to be damaged",
	"an error occured when accessing files",
	"internal error occurred",
	"out of memory" };

class RepairThread;

class Repairer : public Par2::Par2Repairer, public ParChecker::AbstractRepairer
{
public:
	Repairer(ParChecker* owner):
		Par2::Par2Repairer(owner->m_parCout, owner->m_parCerr),
		m_owner(owner), commandLine(owner->m_parCout, owner->m_parCerr) {}
	Par2::Result PreProcess(const char *parFilename);
	Par2::Result Process(bool dorepair);
	virtual Repairer* GetRepairer() { return this; }

protected:
	virtual void sig_filename(std::string filename) { m_owner->signal_filename(filename); }
	virtual void sig_progress(int progress) { m_owner->signal_progress(progress); }
	virtual void sig_done(std::string filename, int available, int total) { m_owner->signal_done(filename, available, total); }

	virtual bool ScanDataFile(Par2::DiskFile *diskfile, Par2::Par2RepairerSourceFile* &sourcefile,
		Par2::MatchType &matchtype, Par2::MD5Hash &hashfull, Par2::MD5Hash &hash16k, Par2::u32 &count);
	virtual bool RepairData(Par2::u32 inputindex, size_t blocklength);

private:
	typedef vector<Thread*> Threads;

	ParChecker* m_owner;
	Par2::CommandLine commandLine;
	Threads m_threads;
	bool m_parallel;
	Mutex progresslock;

	virtual void BeginRepair();
	virtual void EndRepair();
	void RepairBlock(Par2::u32 inputindex, Par2::u32 outputindex, size_t blocklength);
	static void SyncSleep();

	friend class ParChecker;
	friend class RepairThread;
};

class RepairThread : public Thread
{
public:
	RepairThread(Repairer* owner) : m_owner(owner) {}
	void RepairBlock(Par2::u32 inputindex, Par2::u32 outputindex, size_t blocklength);
	bool IsWorking() { return m_working; }

protected:
	virtual void Run();

private:
	Repairer* m_owner;
	Par2::u32 m_inputindex;
	Par2::u32 m_outputindex;
	size_t m_blocklength;
	volatile bool m_working = false;
};

class RepairCreatorPacket : public Par2::CreatorPacket
{
	friend class ParChecker;
};

Par2::Result Repairer::PreProcess(const char *parFilename)
{
	BString<100> memParam("-m%i", g_Options->GetParBuffer());

	if (g_Options->GetParScan() == Options::psFull)
	{
		BString<1024> wildcardParam(parFilename, 1024);
		char* basename = FileSystem::BaseFileName(wildcardParam);
		if (basename != wildcardParam && strlen(basename) > 0)
		{
			basename[0] = '*';
			basename[1] = '\0';
		}

		const char* argv[] = { "par2", "r", "-v", memParam, parFilename, wildcardParam };
		if (!commandLine.Parse(6, (char**)argv))
		{
			return Par2::eInvalidCommandLineArguments;
		}
	}
	else
	{
		const char* argv[] = { "par2", "r", "-v", memParam, parFilename };
		if (!commandLine.Parse(5, (char**)argv))
		{
			return Par2::eInvalidCommandLineArguments;
		}
	}

	return Par2Repairer::PreProcess(commandLine);
}

Par2::Result Repairer::Process(bool dorepair)
{
	Par2::Result res = Par2Repairer::Process(commandLine, dorepair);
	debug("ParChecker: Process-result=%i", res);
	return res;
}


bool Repairer::ScanDataFile(Par2::DiskFile *diskfile, Par2::Par2RepairerSourceFile* &sourcefile,
	Par2::MatchType &matchtype, Par2::MD5Hash &hashfull, Par2::MD5Hash &hash16k, Par2::u32 &count)
{
	if (m_owner->GetParQuick() && sourcefile)
	{
		string path;
		string name;
		Par2::DiskFile::SplitFilename(diskfile->FileName(), path, name);

		sig_filename(name);

		if (!(m_owner->GetStage() == ParChecker::ptVerifyingRepaired && m_owner->GetParFull()))
		{
			int availableBlocks = sourcefile->BlockCount();
			ParChecker::EFileStatus fileStatus = m_owner->VerifyDataFile(diskfile, sourcefile, &availableBlocks);
			if (fileStatus != ParChecker::fsUnknown)
			{
				sig_done(name, availableBlocks, sourcefile->BlockCount());
				sig_progress(1000);
				matchtype = fileStatus == ParChecker::fsSuccess ? Par2::eFullMatch :
					fileStatus == ParChecker::fsPartial ? Par2::ePartialMatch : Par2::eNoMatch;
				m_owner->SetParFull(false);
				return true;
			}
		}
	}

	return Par2Repairer::ScanDataFile(diskfile, sourcefile, matchtype, hashfull, hash16k, count);
}

void Repairer::BeginRepair()
{
	int maxThreads = g_Options->GetParThreads() > 0 ? g_Options->GetParThreads() : Util::NumberOfCpuCores();
	maxThreads = maxThreads > 0 ? maxThreads : 1;

	int threads = maxThreads > (int)missingblockcount ? (int)missingblockcount : maxThreads;

	m_owner->PrintMessage(Message::mkInfo, "Using %i of max %i thread(s) to repair %i block(s) for %s",
		threads, maxThreads, (int)missingblockcount, *m_owner->m_nzbName);

	m_parallel = threads > 1;

	if (m_parallel)
	{
		for (int i = 0; i < threads; i++)
		{
			RepairThread* repairThread = new RepairThread(this);
			m_threads.push_back(repairThread);
			repairThread->SetAutoDestroy(true);
			repairThread->Start();
		}

#ifdef WIN32
		timeBeginPeriod(1);
#endif
	}
}

void Repairer::EndRepair()
{
	if (m_parallel)
	{
		for (Thread* thread : m_threads)
		{
			thread->Stop();
		}

#ifdef WIN32
		timeEndPeriod(1);
#endif
	}
}

bool Repairer::RepairData(Par2::u32 inputindex, size_t blocklength)
{
	if (!m_parallel)
	{
		return false;
	}

	for (Par2::u32 outputindex = 0; outputindex < missingblockcount; )
	{
		bool jobAdded = false;
		for (Thread* thread : m_threads)
		{
			RepairThread* repairThread = (RepairThread*)thread;
			if (!repairThread->IsWorking())
			{
				repairThread->RepairBlock(inputindex, outputindex, blocklength);
				outputindex++;
				jobAdded = true;
				break;
			}
		}

		if (cancelled)
		{
			break;
		}

		if (!jobAdded)
		{
			SyncSleep();
		}
	}

	// Wait until all m_Threads complete their jobs
	bool working = true;
	while (working)
	{
		working = false;
		for (Thread* thread : m_threads)
		{
			RepairThread* repairThread = (RepairThread*)thread;
			if (repairThread->IsWorking())
			{
				working = true;
				SyncSleep();
				break;
			}
		}
	}

	return true;
}

void Repairer::RepairBlock(Par2::u32 inputindex, Par2::u32 outputindex, size_t blocklength)
{
	// Select the appropriate part of the output buffer
	void *outbuf = &((Par2::u8*)outputbuffer)[chunksize * outputindex];

	// Process the data
	rs.Process(blocklength, inputindex, inputbuffer, outputindex, outbuf);

	if (noiselevel > Par2::CommandLine::nlQuiet)
	{
		// Update a progress indicator

		Par2::u32 oldfraction;
		Par2::u32 newfraction;
		{
			Guard guard(progresslock);
			oldfraction = (Par2::u32)(1000 * progress / totaldata);
			progress += blocklength;
			newfraction = (Par2::u32)(1000 * progress / totaldata);
		}

		if (oldfraction != newfraction)
		{
			sig_progress(newfraction);
		}
	}
}

// Sleep for synchronisation
void Repairer::SyncSleep()
{
#ifdef WIN32
	// Windows doesn't allow sleep intervals less than one millisecond
	Sleep(1);
#else
	usleep(100);
#endif
}

void RepairThread::Run()
{
	while (!IsStopped())
	{
		if (m_working)
		{
			m_owner->RepairBlock(m_inputindex, m_outputindex, m_blocklength);
			m_working = false;
		}
		else
		{
			Repairer::SyncSleep();
		}
	}
}

void RepairThread::RepairBlock(Par2::u32 inputindex, Par2::u32 outputindex, size_t blocklength)
{
	m_inputindex = inputindex;
	m_outputindex = outputindex;
	m_blocklength = blocklength;
	m_working = true;
}


int ParChecker::StreamBuf::overflow(int ch)
{
	if (ch == '\n' || ch == '\r')
	{
		char* msg = (char*)*m_buffer;

		// make par2-logging less verbose
		bool extraDebug = !msg || strchr(msg, '%') ||
			!strncmp(msg, "Loading", 7) ||
			(!strncmp(msg, "Target: ", 8) && strcmp(msg + strlen(msg) - 5, "found"));

		if (msg)
		{
			if (!strncmp(msg, "You have ", 9))
			{
				msg += 9;
			}

			if (extraDebug)
			{
				debug("Par: %s", msg);
			}
			else
			{
				m_owner->PrintMessage(m_kind, "Par: %s", msg);
			}
		}

		m_buffer.Clear();
	}
	else
	{
		char bf[2];
		bf[0] = (char)ch;
		bf[1] = '\0';
		m_buffer.Append(bf);
	}
	return (int)ch;
}


void ParChecker::Cleanup()
{
	Guard guard(m_repairerMutex);
	m_repairer.reset();
	m_queuedParFiles.clear();
	m_processedFiles.clear();
	m_sourceFiles.clear();
	m_dupeSources.clear();
	m_errMsg = nullptr;
}

void ParChecker::Execute()
{
	m_status = RunParCheckAll();

	if (m_status == psRepairNotNeeded && m_parQuick && m_forceRepair && !IsStopped())
	{
		PrintMessage(Message::mkInfo, "Performing full par-check for %s", *m_nzbName);
		m_parQuick = false;
		m_status = RunParCheckAll();
	}

	Completed();
}

ParChecker::EStatus ParChecker::RunParCheckAll()
{
	ParParser::ParFileList fileList;
	if (!ParParser::FindMainPars(m_destDir, &fileList))
	{
		PrintMessage(Message::mkError, "Could not start par-check for %s. Could not find any par-files", *m_nzbName);
		return psFailed;
	}

	EStatus allStatus = psRepairNotNeeded;
	m_parFull = true;

	for (CString& parFilename : fileList)
	{
		debug("Found par: %s", *parFilename);

		if (!IsStopped())
		{
			BString<1024> fullParFilename( "%s%c%s", *m_destDir, PATH_SEPARATOR, *parFilename);

			int baseLen = 0;
			ParParser::ParseParFilename(parFilename, true, &baseLen, nullptr);
			BString<1024> infoName;
			infoName.Set(parFilename, baseLen);

			BString<1024> parInfoName("%s%c%s", *m_nzbName, PATH_SEPARATOR, *infoName);
			SetInfoName(parInfoName);

			EStatus status = RunParCheck(fullParFilename);

			// accumulate total status, the worst status has priority
			if (allStatus > status)
			{
				allStatus = status;
			}
		}
	}

	return allStatus;
}

ParChecker::EStatus ParChecker::RunParCheck(const char* parFilename)
{
	Cleanup();
	m_parFilename = parFilename;
	m_stage = ptLoadingPars;
	m_processedCount = 0;
	m_extraFiles = 0;
	m_quickFiles = 0;
	m_verifyingExtraFiles = false;
	m_hasDamagedFiles = false;
	EStatus status = psFailed;

	PrintMessage(Message::mkInfo, "Verifying %s", *m_infoName);

	debug("par: %s", m_parFilename);

	m_progressLabel.Format("Verifying %s", *m_infoName);
	m_fileProgress = 0;
	m_stageProgress = 0;
	UpdateProgress();

	Par2::Result res = (Par2::Result)PreProcessPar();
	if (IsStopped() || res != Par2::eSuccess)
	{
		Cleanup();
		return psFailed;
	}

	CString creator = GetPacketCreator();
	info("Recovery files created by: %s", creator.Empty() ? "<unknown program>" : *creator);

	m_stage = ptVerifyingSources;
	res = GetRepairer()->Process(false);

	if (!m_parQuick)
	{
		CheckEmptyFiles();
	}

	bool addedSplittedFragments = false;
	if (m_hasDamagedFiles && !IsStopped() && res == Par2::eRepairNotPossible)
	{
		addedSplittedFragments = AddSplittedFragments();
		if (addedSplittedFragments)
		{
			res = GetRepairer()->Process(false);
		}
	}

	if (m_hasDamagedFiles && !IsStopped() && GetRepairer()->missingfilecount > 0 &&
		!(addedSplittedFragments && res == Par2::eRepairPossible) &&
		(g_Options->GetParScan() == Options::psExtended ||
		 g_Options->GetParScan() == Options::psDupe))
	{
		if (AddMissingFiles())
		{
			res = GetRepairer()->Process(false);
		}
	}

	if (m_hasDamagedFiles && !IsStopped() && res == Par2::eRepairNotPossible)
	{
		res = (Par2::Result)ProcessMorePars();
	}

	if (m_hasDamagedFiles && !IsStopped() && res == Par2::eRepairNotPossible &&
		g_Options->GetParScan() == Options::psDupe)
	{
		if (AddDupeFiles())
		{
			res = GetRepairer()->Process(false);
			if (!IsStopped() && res == Par2::eRepairNotPossible)
			{
				res = (Par2::Result)ProcessMorePars();
			}
		}
	}

	if (IsStopped())
	{
		Cleanup();
		return psFailed;
	}

	status = psFailed;

	if (res == Par2::eSuccess || !m_hasDamagedFiles)
	{
		PrintMessage(Message::mkInfo, "Repair not needed for %s", *m_infoName);
		status = psRepairNotNeeded;
	}
	else if (res == Par2::eRepairPossible)
	{
		status = psRepairPossible;
		if (g_Options->GetParRepair())
		{
			PrintMessage(Message::mkInfo, "Repairing %s", *m_infoName);

			SaveSourceList();
			m_progressLabel.Format("Repairing %s", *m_infoName);
			m_fileProgress = 0;
			m_stageProgress = 0;
			m_processedCount = 0;
			m_stage = ptRepairing;
			m_filesToRepair = GetRepairer()->damagedfilecount + GetRepairer()->missingfilecount;
			UpdateProgress();

			res = GetRepairer()->Process(true);
			if (res == Par2::eSuccess)
			{
				PrintMessage(Message::mkInfo, "Successfully repaired %s", *m_infoName);
				status = psRepaired;
				StatDupeSources(&m_dupeSources);
				DeleteLeftovers();
			}
			else
			{
				status = psFailed;
			}
		}
		else
		{
			PrintMessage(Message::mkInfo, "Repair possible for %s", *m_infoName);
		}
	}

	if (IsStopped())
	{
		if (m_stage >= ptRepairing)
		{
			PrintMessage(Message::mkWarning, "Repair cancelled for %s", *m_infoName);
			m_errMsg = "repair cancelled";
			status = psRepairPossible;
		}
		else
		{
			PrintMessage(Message::mkWarning, "Par-check cancelled for %s", *m_infoName);
			m_errMsg = "par-check cancelled";
			status = psFailed;
		}
	}
	else if (status == psFailed)
	{
		if (!m_errMsg && (int)res >= 0 && (int)res <= 8)
		{
			m_errMsg = Par2CmdLineErrStr[res];
		}
		PrintMessage(Message::mkError, "Repair failed for %s: %s. Recovery files created by: %s",
			*m_infoName, *m_errMsg, creator.Empty() ? "<unknown program>" : *creator);
	}

	Cleanup();
	return status;
}

int ParChecker::PreProcessPar()
{
	Par2::Result res = Par2::eRepairFailed;
	while (!IsStopped() && res != Par2::eSuccess)
	{
		Cleanup();

		{
			Guard guard(m_repairerMutex);
			m_repairer = std::make_unique<Repairer>(this);
		}

		res = GetRepairer()->PreProcess(m_parFilename);
		debug("ParChecker: PreProcess-result=%i", res);

		if (IsStopped())
		{
			PrintMessage(Message::mkError, "Could not verify %s: stopping", *m_infoName);
			m_errMsg = "par-check was stopped";
			return Par2::eRepairFailed;
		}

		if (res == Par2::eInvalidCommandLineArguments)
		{
			PrintMessage(Message::mkError, "Could not start par-check for %s. Par-file: %s", *m_infoName, m_parFilename);
			m_errMsg = "Command line could not be parsed";
			return res;
		}

		if (res != Par2::eSuccess)
		{
			PrintMessage(Message::mkWarning, "Could not verify %s: par2-file could not be processed", *m_infoName);
			PrintMessage(Message::mkInfo, "Requesting more par2-files for %s", *m_infoName);
			bool hasMorePars = LoadMainParBak();
			if (!hasMorePars)
			{
				PrintMessage(Message::mkWarning, "No more par2-files found");
				break;
			}
		}
	}

	if (res != Par2::eSuccess)
	{
		PrintMessage(Message::mkError, "Could not verify %s: par2-file could not be processed", *m_infoName);
		m_errMsg = "par2-file could not be processed";
		return res;
	}

	return res;
}

bool ParChecker::LoadMainParBak()
{
	while (!IsStopped())
	{
		bool hasMorePars = false;
		{
			Guard guard(m_queuedParFilesMutex);
			hasMorePars = !m_queuedParFiles.empty();
			m_queuedParFiles.clear();
		}

		if (hasMorePars)
		{
			return true;
		}

		int blockFound = 0;
		bool requested = RequestMorePars(1, &blockFound);
		if (requested)
		{
			m_progressLabel = "Awaiting additional par-files";
			m_fileProgress = 0;
			UpdateProgress();
		}

		{
			Guard guard(m_queuedParFilesMutex);
			hasMorePars = !m_queuedParFiles.empty();
			m_queuedParFilesChanged = false;
		}

		if (!requested && !hasMorePars)
		{
			return false;
		}

		if (!hasMorePars)
		{
			// wait until new files are added by "AddParFile" or a change is signaled by "QueueChanged"
			bool queuedParFilesChanged = false;
			while (!queuedParFilesChanged && !IsStopped())
			{
				{
					Guard guard(m_queuedParFilesMutex);
					queuedParFilesChanged = m_queuedParFilesChanged;
				}
				Util::Sleep(100);
			}
		}
	}

	return false;
}

int ParChecker::ProcessMorePars()
{
	Par2::Result res = Par2::eRepairNotPossible;

	bool moreFilesLoaded = true;
	while (!IsStopped() && res == Par2::eRepairNotPossible)
	{
		int missingblockcount = GetRepairer()->missingblockcount -
			GetRepairer()->recoverypacketmap.size();
		if (missingblockcount <= 0)
		{
			return Par2::eRepairPossible;
		}

		if (moreFilesLoaded)
		{
			PrintMessage(Message::mkInfo, "Need more %i par-block(s) for %s", missingblockcount, *m_infoName);
		}

		bool hasMorePars;
		{
			Guard guard(m_queuedParFilesMutex);
			hasMorePars = !m_queuedParFiles.empty();
		}

		if (!hasMorePars)
		{
			int blockFound = 0;
			bool requested = RequestMorePars(missingblockcount, &blockFound);
			if (requested)
			{
				m_progressLabel = "Awaiting additional par-files";
				m_fileProgress = 0;
				UpdateProgress();
			}

			{
				Guard guard(m_queuedParFilesMutex);
				hasMorePars = !m_queuedParFiles.empty();
				m_queuedParFilesChanged = false;
			}

			if (!requested && !hasMorePars)
			{
				m_errMsg.Format("not enough par-blocks, %i block(s) needed, but %i block(s) available", missingblockcount, blockFound);
				break;
			}

			if (!hasMorePars)
			{
				// wait until new files are added by "AddParFile" or a change is signaled by "QueueChanged"
				bool queuedParFilesChanged = false;
				while (!queuedParFilesChanged && !IsStopped())
				{
					{
						Guard guard(m_queuedParFilesMutex);
						queuedParFilesChanged = m_queuedParFilesChanged;
					}
					Util::Sleep(100);
				}
			}
		}

		if (IsStopped())
		{
			break;
		}

		moreFilesLoaded = LoadMorePars();
		if (moreFilesLoaded)
		{
			GetRepairer()->UpdateVerificationResults();
			res = GetRepairer()->Process(false);
		}
	}

	return res;
}

bool ParChecker::LoadMorePars()
{
	FileList moreFiles;
	{
		Guard guard(m_queuedParFilesMutex);
		moreFiles = std::move(m_queuedParFiles);
		m_queuedParFiles.clear();
	}

	for (CString& parFilename : moreFiles)
	{
		bool loadedOK = GetRepairer()->LoadPacketsFromFile(*parFilename);
		if (loadedOK)
		{
			PrintMessage(Message::mkInfo, "File %s successfully loaded for par-check", FileSystem::BaseFileName(parFilename));
		}
		else
		{
			PrintMessage(Message::mkInfo, "Could not load file %s for par-check", FileSystem::BaseFileName(parFilename));
		}
	}

	return !moreFiles.empty();
}

void ParChecker::AddParFile(const char * parFilename)
{
	Guard guard(m_queuedParFilesMutex);
	m_queuedParFiles.push_back(parFilename);
	m_queuedParFilesChanged = true;
}

void ParChecker::QueueChanged()
{
	Guard guard(m_queuedParFilesMutex);
	m_queuedParFilesChanged = true;
}

bool ParChecker::AddSplittedFragments()
{
	std::list<Par2::CommandLine::ExtraFile> extrafiles;

	DirBrowser dir(m_destDir);
	while (const char* filename = dir.Next())
	{
		if (!IsParredFile(filename) && !IsProcessedFile(filename))
		{
			for (Par2::Par2RepairerSourceFile *sourcefile : GetRepairer()->sourcefiles)
			{
				std::string target = sourcefile->TargetFileName();
				const char* current = FileSystem::BaseFileName(target.c_str());

				// if file was renamed by par-renamer we also check the original filename
				const char* original = FindFileOrigname(current);

				if (MaybeSplittedFragement(filename, current) ||
					(!Util::EmptyStr(original) && strcasecmp(original, current) &&
					MaybeSplittedFragement(filename, original)))
				{
					detail("Found splitted fragment %s", filename);
					BString<1024> fullfilename("%s%c%s", *m_destDir, PATH_SEPARATOR, filename);
					Par2::CommandLine::ExtraFile extrafile(*fullfilename, FileSystem::FileSize(fullfilename));
					extrafiles.push_back(extrafile);
					break;
				}
			}
		}
	}

	bool fragmentsAdded = false;

	if (!extrafiles.empty())
	{
		m_extraFiles += extrafiles.size();
		m_verifyingExtraFiles = true;
		PrintMessage(Message::mkInfo, "Found %i splitted fragments for %s", (int)extrafiles.size(), *m_infoName);
		fragmentsAdded = GetRepairer()->VerifyExtraFiles(extrafiles);
		GetRepairer()->UpdateVerificationResults();
		m_verifyingExtraFiles = false;
	}

	return fragmentsAdded;
}

bool ParChecker::MaybeSplittedFragement(const char* filename1, const char* filename2)
{
	// check if name is same but the first name has additional numerical extension
	int len = strlen(filename2);
	if (!strncasecmp(filename1, filename2, len))
	{
		const char* p = filename1 + len;
		if (*p == '.')
		{
			for (p++; *p && strchr("0123456789", *p); p++) ;
			if (!*p)
			{
				return true;
			}
		}
	}

	// check if same name (without extension) and extensions are numerical and exactly 3 characters long
	const char* ext1 = strrchr(filename1, '.');
	const char* ext2 = strrchr(filename2, '.');
	if (ext1 && ext2 && (strlen(ext1) == 4) && (strlen(ext2) == 4) &&
		!strncasecmp(filename1, filename2, ext1 - filename1))
	{
		for (ext1++; *ext1 && strchr("0123456789", *ext1); ext1++) ;
		for (ext2++; *ext2 && strchr("0123456789", *ext2); ext2++) ;
		if (!*ext1 && !*ext2)
		{
			return true;
		}
	}

	return false;
}

bool ParChecker::AddMissingFiles()
{
	return AddExtraFiles(true, false, m_destDir);
}

bool ParChecker::AddDupeFiles()
{
	BString<1024> directory = m_parFilename;

	bool added = AddExtraFiles(false, false, directory);

	if (GetRepairer()->missingblockcount > 0)
	{
		// scanning directories of duplicates
		RequestDupeSources(&m_dupeSources);

		if (!m_dupeSources.empty())
		{
			int wasBlocksMissing = GetRepairer()->missingblockcount;

			for (DupeSource& dupeSource : m_dupeSources)
			{
				if (GetRepairer()->missingblockcount > 0 && FileSystem::DirectoryExists(dupeSource.GetDirectory()))
				{
					int wasBlocksMissing2 = GetRepairer()->missingblockcount;
					bool oneAdded = AddExtraFiles(false, true, dupeSource.GetDirectory());
					added |= oneAdded;
					int blocksMissing2 = GetRepairer()->missingblockcount;
					dupeSource.SetUsedBlocks(dupeSource.GetUsedBlocks() + (wasBlocksMissing2 - blocksMissing2));
				}
			}

			int blocksMissing = GetRepairer()->missingblockcount;
			if (blocksMissing < wasBlocksMissing)
			{
				PrintMessage(Message::mkInfo, "Found extra %i blocks in dupe sources", wasBlocksMissing - blocksMissing);
			}
			else
			{
				PrintMessage(Message::mkInfo, "No extra blocks found in dupe sources");
			}
		}
	}

	return added;
}

/*
* Files with the same name as in par-file (and a differnt extension) are
* placed at the top of the list to be scanned first.
*/
void ParChecker::SortExtraFiles(void* extrafiles)
{
	CString baseParFilename = FileSystem::BaseFileName(m_parFilename);
	if (char* ext = strrchr(baseParFilename, '.')) *ext = '\0'; // trim extension

	((std::list<Par2::CommandLine::ExtraFile>*)extrafiles)->sort(
		[&baseParFilename](Par2::CommandLine::ExtraFile& file1, Par2::CommandLine::ExtraFile& file2)
		{
			BString<1024> name1 = FileSystem::BaseFileName(file1.FileName().c_str());
			if (char* ext = strrchr(name1, '.')) *ext = '\0'; // trim extension

			BString<1024> name2 = FileSystem::BaseFileName(file2.FileName().c_str());
			if (char* ext = strrchr(name2, '.')) *ext = '\0'; // trim extension

			return strcmp(name1, baseParFilename) == 0 && strcmp(name1, name2) != 0;
		});
}

bool ParChecker::AddExtraFiles(bool onlyMissing, bool externalDir, const char* directory)
{
	if (externalDir)
	{
		PrintMessage(Message::mkInfo, "Performing dupe par-scan for %s in %s", *m_infoName, FileSystem::BaseFileName(directory));
	}
	else
	{
		PrintMessage(Message::mkInfo, "Performing extra par-scan for %s", *m_infoName);
	}

	std::list<Par2::CommandLine::ExtraFile> extrafiles;

	DirBrowser dir(directory);
	while (const char* filename = dir.Next())
	{
		if (externalDir || (!IsParredFile(filename) && !IsProcessedFile(filename)))
		{
			BString<1024> fullfilename("%s%c%s", directory, PATH_SEPARATOR, filename);
			extrafiles.emplace_back(*fullfilename, FileSystem::FileSize(fullfilename));
		}
	}

	SortExtraFiles(&extrafiles);

	// Scan files
	bool filesAdded = false;
	if (!extrafiles.empty())
	{
		m_extraFiles += extrafiles.size();
		m_verifyingExtraFiles = true;

		// adding files one by one until all missing files are found

		while (!IsStopped() && extrafiles.size() > 0)
		{
			std::list<Par2::CommandLine::ExtraFile> extrafiles1;
			extrafiles1.splice(extrafiles1.end(), extrafiles, extrafiles.begin());

			Par2::CommandLine::ExtraFile& extraFile = extrafiles1.front();

			int wasFilesMissing = GetRepairer()->missingfilecount;
			int wasBlocksMissing = GetRepairer()->missingblockcount;

			GetRepairer()->VerifyExtraFiles(extrafiles1);
			GetRepairer()->UpdateVerificationResults();

			bool fileAdded = wasFilesMissing > (int)GetRepairer()->missingfilecount;
			bool blockAdded = wasBlocksMissing > (int)GetRepairer()->missingblockcount;

			if (fileAdded && !externalDir)
			{
				PrintMessage(Message::mkInfo, "Found missing file %s", FileSystem::BaseFileName(extraFile.FileName().c_str()));
				RegisterParredFile(FileSystem::BaseFileName(extraFile.FileName().c_str()));
			}
			else if (blockAdded)
			{
				PrintMessage(Message::mkInfo, "Found %i missing blocks", wasBlocksMissing - (int)GetRepairer()->missingblockcount);
			}

			filesAdded |= fileAdded | blockAdded;

			if (onlyMissing && GetRepairer()->missingfilecount == 0)
			{
				PrintMessage(Message::mkInfo, "All missing files found, aborting par-scan");
				break;
			}

			if (!onlyMissing && GetRepairer()->missingblockcount == 0)
			{
				PrintMessage(Message::mkInfo, "All missing blocks found, aborting par-scan");
				break;
			}
		}

		m_verifyingExtraFiles = false;
	}

	return filesAdded;
}

bool ParChecker::IsProcessedFile(const char* filename)
{
	for (CString& processedFilename : m_processedFiles)
	{
		if (!strcasecmp(FileSystem::BaseFileName(processedFilename), filename))
		{
			return true;
		}
	}

	return false;
}

void ParChecker::signal_filename(std::string str)
{
	if (!m_lastFilename.compare(str))
	{
		return;
	}

	m_lastFilename = str;

	const char* stageMessage[] = { "Loading file", "Verifying file", "Repairing file", "Verifying repaired file" };

	if (m_stage == ptRepairing)
	{
		m_stage = ptVerifyingRepaired;
	}

	// don't print progress messages when verifying repaired files in quick verification mode,
	// because repaired files are not verified in this mode
	if (!(m_stage == ptVerifyingRepaired && m_parQuick))
	{
		PrintMessage(Message::mkInfo, "%s %s", stageMessage[m_stage], str.c_str());
	}

	if (m_stage == ptLoadingPars || m_stage == ptVerifyingSources)
	{
		m_processedFiles.push_back(str.c_str());
	}

	m_progressLabel.Format("%s %s", stageMessage[m_stage], str.c_str());
	m_fileProgress = 0;
	UpdateProgress();
}

void ParChecker::signal_progress(int progress)
{
	m_fileProgress = (int)progress;

	if (m_stage == ptRepairing)
	{
		// calculating repair-data for all files
		m_stageProgress = m_fileProgress;
	}
	else
	{
		// processing individual files

		int totalFiles = 0;
		int processedFiles = m_processedCount;
		if (m_stage == ptVerifyingRepaired)
		{
			// repairing individual files
			totalFiles = m_filesToRepair;
		}
		else
		{
			// verifying individual files
			totalFiles = GetRepairer()->sourcefiles.size() + m_extraFiles;
			if (m_extraFiles > 0)
			{
				// during extra par scan don't count quickly verified files;
				// extra files require much more time for verification;
				// counting only fully scanned files improves estimated time accuracy.
				totalFiles -= m_quickFiles;
				processedFiles -= m_quickFiles;
			}
		}

		if (totalFiles > 0)
		{
			if (m_fileProgress < 1000)
			{
				m_stageProgress = (processedFiles * 1000 + m_fileProgress) / totalFiles;
			}
			else
			{
				m_stageProgress = processedFiles * 1000 / totalFiles;
			}
		}
		else
		{
			m_stageProgress = 0;
		}
	}

	debug("Current-progress: %i, Total-progress: %i", m_fileProgress, m_stageProgress);

	UpdateProgress();
}

void ParChecker::signal_done(std::string str, int available, int total)
{
	m_processedCount++;

	if (m_stage == ptVerifyingSources)
	{
		if (available < total && !m_verifyingExtraFiles)
		{
			const char* filename = str.c_str();

			bool fileExists = true;
			for (Par2::Par2RepairerSourceFile* sourcefile : GetRepairer()->sourcefiles)
			{
				if (sourcefile && !strcmp(filename, FileSystem::BaseFileName(sourcefile->TargetFileName().c_str())) &&
					!sourcefile->GetTargetExists())
				{
					fileExists = false;
					break;
				}
			}

			bool ignore = Util::MatchFileExt(filename, g_Options->GetParIgnoreExt(), ",;");
			m_hasDamagedFiles |= !ignore;

			if (fileExists)
			{
				PrintMessage(Message::mkWarning, "File %s has %i bad block(s) of total %i block(s)%s",
					filename, total - available, total, ignore ? ", ignoring" : "");
			}
			else
			{
				PrintMessage(Message::mkWarning, "File %s with %i block(s) is missing%s",
					filename, total, ignore ? ", ignoring" : "");
			}

			if (!IsProcessedFile(filename))
			{
				m_processedFiles.push_back(filename);
			}
		}
	}
}

/*
 * Only if ParQuick isn't enabled:
 * For empty damaged files the callback-function "signal_done" isn't called and the flag "m_bHasDamagedFiles"
 * therefore isn't set. In this function we expicitly check such files.
 */
void ParChecker::CheckEmptyFiles()
{
	for (Par2::Par2RepairerSourceFile* sourcefile : GetRepairer()->sourcefiles)
	{
		if (sourcefile && sourcefile->GetDescriptionPacket())
		{
			// GetDescriptionPacket()->FileName() returns a temp string object, which we need to hold for a while
			std::string filenameObj = Par2::DiskFile::TranslateFilename(sourcefile->GetDescriptionPacket()->FileName());
			const char* filename = filenameObj.c_str();
			if (!Util::EmptyStr(filename) && !IsProcessedFile(filename))
			{
				bool ignore = Util::MatchFileExt(filename, g_Options->GetParIgnoreExt(), ",;");
				m_hasDamagedFiles |= !ignore;

				int total = sourcefile->GetVerificationPacket() ? sourcefile->GetVerificationPacket()->BlockCount() : 0;
				PrintMessage(Message::mkWarning, "File %s has %i bad block(s) of total %i block(s)%s",
					filename, total, total, ignore ? ", ignoring" : "");
			}
		}
		else
		{
			m_hasDamagedFiles = true;
		}
	}
}

void ParChecker::Cancel()
{
	{
		Guard guard(m_repairerMutex);
		if (m_repairer)
		{
			m_repairer->GetRepairer()->cancelled = true;
		}
	}
	QueueChanged();
}

void ParChecker::SaveSourceList()
{
	// Buliding a list of DiskFile-objects, marked as source-files

	for (Par2::Par2RepairerSourceFile* sourcefile : GetRepairer()->sourcefiles)
	{
		vector<Par2::DataBlock>::iterator it2 = sourcefile->SourceBlocks();
		for (int i = 0; i < (int)sourcefile->BlockCount(); i++, it2++)
		{
			Par2::DataBlock block = *it2;
			Par2::DiskFile* sourceFile = block.GetDiskFile();
			if (sourceFile &&
				std::find(m_sourceFiles.begin(), m_sourceFiles.end(), sourceFile) == m_sourceFiles.end())
			{
				m_sourceFiles.push_back(sourceFile);
			}
		}
	}
}

void ParChecker::DeleteLeftovers()
{
	// After repairing check if all DiskFile-objects saved by "SaveSourceList()" have
	// corresponding target-files. If not - the source file was replaced. In this case
	// the DiskFile-object points to the renamed bak-file, which we can delete.

	for (void* sf : m_sourceFiles)
	{
		Par2::DiskFile* sourceFile = (Par2::DiskFile*)sf;

		bool found = false;
		for (Par2::Par2RepairerSourceFile* sourcefile : GetRepairer()->sourcefiles)
		{
			if (sourcefile->GetTargetFile() == sourceFile)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			PrintMessage(Message::mkInfo, "Deleting file %s", FileSystem::BaseFileName(sourceFile->FileName().c_str()));
			FileSystem::DeleteFile(sourceFile->FileName().c_str());
		}
	}
}

/**
 * This function implements quick par verification replacing the standard verification routine
 * from libpar2:
 * - for successfully downloaded files the function compares CRC of the file computed during
 *   download with CRC stored in PAR2-file;
 * - for partially downloaded files the CRCs of articles are compared with block-CRCs stored
 *   in PAR2-file;
 * - for completely failed files (not a single successful article) no verification is needed at all.
 *
 * Limitation of the function:
 * This function requires every block in the file to have an unique CRC (across all blocks
 * of the par-set). Otherwise the full verification is performed.
 * The limitation can be avoided by using something more smart than "verificationhashtable.Lookup"
 * but in the real life all blocks have unique CRCs and the simple "Lookup" works good enough.
 */
ParChecker::EFileStatus ParChecker::VerifyDataFile(void* diskfile, void* sourcefile, int* availableBlocks)
{
	if (m_stage != ptVerifyingSources)
	{
		// skipping verification for repaired files, assuming the files were correctly repaired,
		// the only reason for incorrect files after repair are hardware errors (memory, disk),
		// but this isn't something NZBGet should care about.
		return fsSuccess;
	}

	Par2::DiskFile* diskFile = (Par2::DiskFile*)diskfile;
	Par2::Par2RepairerSourceFile* sourceFile = (Par2::Par2RepairerSourceFile*)sourcefile;
	if (!sourcefile || !sourceFile->GetTargetExists())
	{
		return fsUnknown;
	}

	Par2::VerificationPacket* packet = sourceFile->GetVerificationPacket();
	if (!packet)
	{
		return fsUnknown;
	}

	std::string filenameObj = sourceFile->GetTargetFile()->FileName();
	const char* filename = filenameObj.c_str();

	if (FileSystem::FileSize(filename) == 0 && sourceFile->BlockCount() > 0)
	{
		*availableBlocks = 0;
		return fsFailure;
	}

	// find file status and CRC computed during download
	uint32 downloadCrc;
	SegmentList segments;
	EFileStatus	fileStatus = FindFileCrc(FileSystem::BaseFileName(filename), &downloadCrc, &segments);
	ValidBlocks validBlocks;

	if (fileStatus == fsFailure || fileStatus == fsUnknown)
	{
		return fileStatus;
	}
	else if ((fileStatus == fsSuccess && !VerifySuccessDataFile(diskfile, sourcefile, downloadCrc)) ||
		(fileStatus == fsPartial && !VerifyPartialDataFile(diskfile, sourcefile, &segments, &validBlocks)))
	{
		PrintMessage(Message::mkWarning, "Quick verification failed for %s file %s, performing full verification instead",
			fileStatus == fsSuccess ? "good" : "damaged", FileSystem::BaseFileName(filename));
		return fsUnknown; // let libpar2 do the full verification of the file
	}

	// attach verification blocks to the file
	*availableBlocks = 0;
	Par2::u64 blocksize = GetRepairer()->mainpacket->BlockSize();
	std::deque<const Par2::VerificationHashEntry*> undoList;
	for (uint32 i = 0; i < packet->BlockCount(); i++)
	{
		if (fileStatus == fsSuccess || validBlocks.at(i))
		{
			const Par2::FILEVERIFICATIONENTRY* entry = packet->VerificationEntry(i);
			Par2::u32 blockCrc = entry->crc;

			// Look for a match
			const Par2::VerificationHashEntry* hashEntry = GetRepairer()->verificationhashtable.Lookup(blockCrc);
			if (!hashEntry || hashEntry->SourceFile() != sourceFile || hashEntry->IsSet())
			{
				// no match found, revert back the changes made by "pHashEntry->SetBlock"
				for (const Par2::VerificationHashEntry* undoEntry : undoList)
				{
					undoEntry->SetBlock(nullptr, 0);
				}
				return fsUnknown;
			}

			undoList.push_back(hashEntry);
			hashEntry->SetBlock(diskFile, i*blocksize);
			(*availableBlocks)++;
		}
	}

	m_quickFiles++;
	PrintMessage(Message::mkDetail, "Quickly verified %s file %s",
		fileStatus == fsSuccess ? "good" : "damaged", FileSystem::BaseFileName(filename));

	return fileStatus;
}

bool ParChecker::VerifySuccessDataFile(void* diskfile, void* sourcefile, uint32 downloadCrc)
{
	Par2::Par2RepairerSourceFile* sourceFile = (Par2::Par2RepairerSourceFile*)sourcefile;
	Par2::u64 blocksize = GetRepairer()->mainpacket->BlockSize();
	Par2::VerificationPacket* packet = sourceFile->GetVerificationPacket();

	// extend lDownloadCrc to block size
	downloadCrc = Par2::CRCUpdateBlock(downloadCrc ^ 0xFFFFFFFF,
		(size_t)(blocksize * packet->BlockCount() > sourceFile->GetTargetFile()->FileSize() ?
			blocksize * packet->BlockCount() - sourceFile->GetTargetFile()->FileSize() : 0)
		) ^ 0xFFFFFFFF;
	debug("Download-CRC: %.8x", downloadCrc);

	// compute file CRC using CRCs of blocks
	uint32 parCrc = 0;
	for (uint32 i = 0; i < packet->BlockCount(); i++)
	{
		const Par2::FILEVERIFICATIONENTRY* entry = packet->VerificationEntry(i);
		Par2::u32 blockCrc = entry->crc;
		parCrc = i == 0 ? blockCrc : Crc32::Combine(parCrc, blockCrc, (uint32)blocksize);
	}
	debug("Block-CRC: %x, filename: %s", parCrc, FileSystem::BaseFileName(sourceFile->GetTargetFile()->FileName().c_str()));

	return parCrc == downloadCrc;
}

bool ParChecker::VerifyPartialDataFile(void* diskfile, void* sourcefile, SegmentList* segments, ValidBlocks* validBlocks)
{
	Par2::Par2RepairerSourceFile* sourceFile = (Par2::Par2RepairerSourceFile*)sourcefile;
	Par2::VerificationPacket* packet = sourceFile->GetVerificationPacket();
	int64 blocksize = GetRepairer()->mainpacket->BlockSize();
	std::string filenameObj = sourceFile->GetTargetFile()->FileName();
	const char* filename = filenameObj.c_str();
	int64 fileSize = sourceFile->GetTargetFile()->FileSize();

	// determine presumably valid and bad blocks based on article download status
	validBlocks->resize(packet->BlockCount(), false);
	for (int i = 0; i < (int)validBlocks->size(); i++)
	{
		int64 blockStart = i * blocksize;
		int64 blockEnd = blockStart + blocksize < fileSize - 1 ? blockStart + blocksize : fileSize - 1;
		bool blockOK = false;
		bool blockEndFound = false;
		int64 curOffset = 0;
		for (Segment& segment : segments)
		{
			if (!blockOK && segment.GetSuccess() && segment.GetOffset() <= blockStart &&
				segment.GetOffset() + segment.GetSize() >= blockStart)
			{
				blockOK = true;
				curOffset = segment.GetOffset();
			}
			if (blockOK)
			{
				if (!(segment.GetSuccess() && segment.GetOffset() == curOffset))
				{
					blockOK = false;
					break;
				}
				if (segment.GetOffset() + segment.GetSize() >= blockEnd)
				{
					blockEndFound = true;
					break;
				}
				curOffset = segment.GetOffset() + segment.GetSize();
			}
		}
		validBlocks->at(i) = blockOK && blockEndFound;
	}

	DiskFile infile;
	if (!infile.Open(filename, DiskFile::omRead))
	{
		PrintMessage(Message::mkError, "Could not open file %s: %s",
			filename, *FileSystem::GetLastErrorMessage());
	}

	// For each sequential range of presumably valid blocks:
	// - compute par-CRC of the range of blocks using block CRCs;
	// - compute download-CRC for the same byte range using CRCs of articles; if articles and block
	//   overlap - read a little bit of data from the file and calculate its CRC;
	// - compare two CRCs - they must match; if not - the file is more damaged than we thought -
	//   let libpar2 do the full verification of the file in this case.
	uint32 parCrc = 0;
	int blockStart = -1;
	validBlocks->push_back(false); // end marker
	for (int i = 0; i < (int)validBlocks->size(); i++)
	{
		bool validBlock = validBlocks->at(i);
		if (validBlock)
		{
			if (blockStart == -1)
			{
				blockStart = i;
			}
			const Par2::FILEVERIFICATIONENTRY* entry = packet->VerificationEntry(i);
			Par2::u32 blockCrc = entry->crc;
			parCrc = blockStart == i ? blockCrc : Crc32::Combine(parCrc, blockCrc, (uint32)blocksize);
		}
		else
		{
			if (blockStart > -1)
			{
				int blockEnd = i - 1;
				int64 bytesStart = blockStart * blocksize;
				int64 bytesEnd = blockEnd * blocksize + blocksize - 1;
				uint32 downloadCrc = 0;
				bool ok = SmartCalcFileRangeCrc(infile, bytesStart,
					bytesEnd < fileSize - 1 ? bytesEnd : fileSize - 1, segments, &downloadCrc);
				if (ok && bytesEnd > fileSize - 1)
				{
					// for the last block: extend lDownloadCrc to block size
					downloadCrc = Par2::CRCUpdateBlock(downloadCrc ^ 0xFFFFFFFF, (size_t)(bytesEnd - (fileSize - 1))) ^ 0xFFFFFFFF;
				}

				if (!ok || downloadCrc != parCrc)
				{
					infile.Close();
					return false;
				}
			}
			blockStart = -1;
		}
	}

	infile.Close();

	return true;
}

/*
 * Compute CRC of bytes range of file using CRCs of segments and reading some data directly
 * from file if necessary
 */
bool ParChecker::SmartCalcFileRangeCrc(DiskFile& file, int64 start, int64 end, SegmentList* segments,
	uint32* downloadCrcOut)
{
	uint32 downloadCrc = 0;
	bool started = false;
	for (Segment& segment : segments)
	{
		if (!started && segment.GetOffset() > start)
		{
			// read start of range from file
			if (!DumbCalcFileRangeCrc(file, start, segment.GetOffset() - 1, &downloadCrc))
			{
				return false;
			}
			if (segment.GetOffset() + segment.GetSize() >= end)
			{
				break;
			}
			started = true;
		}

		if (segment.GetOffset() >= start && segment.GetOffset() + segment.GetSize() <= end)
		{
			downloadCrc = !started ? segment.GetCrc() : Crc32::Combine(downloadCrc, segment.GetCrc(), (uint32)segment.GetSize());
			started = true;
		}

		if (segment.GetOffset() + segment.GetSize() == end)
		{
			break;
		}

		if (segment.GetOffset() + segment.GetSize() > end)
		{
			// read end of range from file
			uint32 partialCrc = 0;
			if (!DumbCalcFileRangeCrc(file, segment.GetOffset(), end, &partialCrc))
			{
				return false;
			}

			downloadCrc = Crc32::Combine(downloadCrc, (uint32)partialCrc, (uint32)(end - segment.GetOffset() + 1));

			break;
		}
	}

	*downloadCrcOut = downloadCrc;
	return true;
}

/*
 * Compute CRC of bytes range of file reading the data directly from file
 */
bool ParChecker::DumbCalcFileRangeCrc(DiskFile& file, int64 start, int64 end, uint32* downloadCrcOut)
{
	if (!file.Seek(start))
	{
		return false;
	}

	CharBuffer buffer(1024 * 64);
	Crc32 downloadCrc;

	int cnt = buffer.Size();
	while (cnt == buffer.Size() && start < end)
	{
		int needBytes = end - start + 1 > buffer.Size() ? buffer.Size() : (int)(end - start + 1);
		cnt = (int)file.Read(buffer, needBytes);
		downloadCrc.Append((uchar*)(char*)buffer, cnt);
		start += cnt;
	}

	*downloadCrcOut = downloadCrc.Finish();
	return true;
}

CString ParChecker::GetPacketCreator()
{
	Par2::CREATORPACKET* creatorpacket;
	if (GetRepairer()->creatorpacket &&
		(creatorpacket = (Par2::CREATORPACKET*)(((RepairCreatorPacket*)GetRepairer()->creatorpacket)->packetdata)))
	{
		int len = (int)(creatorpacket->header.length - sizeof(Par2::PACKET_HEADER));
		BString<1024> creator;
		if (len > 0)
		{
			creator.Set((const char*)creatorpacket->client, len);
		}
		return *creator;
	}

	return nullptr;
}

#endif
