/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Log.h"
#include "FileSystem.h"
#include "Maintenance.h"
#include "Options.h"

extern void ExitProc();
extern int g_ArgumentCount;
extern char* (*g_Arguments)[];


#ifdef HAVE_OPENSSL
class Signature
{
public:
	Signature(const char* inFilename, const char* sigFilename, const char* pubKeyFilename);
	~Signature();
	bool Verify();

private:
	const char* m_inFilename;
	const char* m_sigFilename;
	const char* m_pubKeyFilename;

    EVP_MD_CTX*     m_sha256Ctx;
	uchar           m_sha256[EVP_MAX_MD_SIZE];
    unsigned int    m_sha256Len;
	uchar           m_signature[256];
    EVP_PKEY_CTX*   m_pubKeyCtx;

	bool ComputeSHA256();
	bool ReadSignature();
	bool ReadPublicKey();
};
#endif


Maintenance::~Maintenance()
{
	bool waitScript = false;
	{
		Guard guard(m_controllerMutex);
		if (m_updateScriptController)
		{
			m_updateScriptController->Detach();
			waitScript = true;
		}
	}

	if (waitScript)
	{
		while (m_updateScriptController)
		{
			Util::Sleep(20);
		}
	}
}

void Maintenance::ResetUpdateController()
{
	Guard guard(m_controllerMutex);
	m_updateScriptController = nullptr;
}

void Maintenance::AddMessage(Message::EKind kind, time_t time, const char * text)
{
	if (time == 0)
	{
		time = Util::CurrentTime();
	}

	Guard guard(m_logMutex);
	m_messages.emplace_back(++m_idMessageGen, kind, time, text);
}

bool Maintenance::StartUpdate(EBranch branch)
{
	bool alreadyUpdating;
	{
		Guard guard(m_controllerMutex);
		alreadyUpdating = m_updateScriptController != nullptr;
	}

	if (alreadyUpdating)
	{
		error("Could not start update-script: update-script is already running");
		return false;
	}

	if (!ReadPackageInfoStr("install-script", m_updateScript))
	{
		return false;
	}

	// make absolute path
	if (m_updateScript[0] != PATH_SEPARATOR
#ifdef WIN32
		&& !(strlen(m_updateScript) > 2 && m_updateScript[1] == ':')
#endif
		)
	{
		m_updateScript = CString::FormatStr("%s%c%s", g_Options->GetAppDir(), PATH_SEPARATOR, *m_updateScript);
	}

	m_messages.clear();

	m_updateScriptController = new UpdateScriptController();
	m_updateScriptController->SetArgs({*m_updateScript});
	m_updateScriptController->SetBranch(branch);
	m_updateScriptController->SetAutoDestroy(true);

	m_updateScriptController->Start();

	return true;
}

bool Maintenance::CheckUpdates(CString& updateInfo)
{
	CString updateInfoScript;
	if (!ReadPackageInfoStr("update-info-script", updateInfoScript))
	{
		return false;
	}

	UpdateInfoScriptController::ExecuteScript(updateInfoScript, updateInfo);

	return updateInfo.Length() > 0;
}

bool Maintenance::ReadPackageInfoStr(const char* key, CString& value)
{
	BString<1024> fileName("%s%cpackage-info.json", g_Options->GetWebDir(), PATH_SEPARATOR);

	CharBuffer packageInfo;
	if (!FileSystem::LoadFileIntoBuffer(fileName, packageInfo, true))
	{
		error("Could not load file %s", *fileName);
		return false;
	}

	BString<100> keyStr("\"%s\"", key);

	char* p = strstr(packageInfo, keyStr);
	if (!p)
	{
		error("Could not parse file %s", *fileName);
		return false;
	}

	p = strchr(p + strlen(keyStr), '"');
	if (!p)
	{
		error("Could not parse file %s", *fileName);
		return false;
	}

	p++;
	char* pend = strchr(p, '"');
	if (!pend)
	{
		error("Could not parse file %s", *fileName);
		return false;
	}

	size_t len = pend - p;
	if (len >= sizeof(fileName))
	{
		error("Could not parse file %s", *fileName);
		return false;
	}

	value.Reserve(len);
	strncpy(value, p, len);
	value[len] = '\0';

	WebUtil::JsonDecode(value);

	return true;
}

bool Maintenance::VerifySignature(const char* inFilename, const char* sigFilename, const char* pubKeyFilename)
{
#ifdef HAVE_OPENSSL
	Signature signature(inFilename, sigFilename, pubKeyFilename);
	return signature.Verify();
#else
	return false;
#endif
}

void UpdateScriptController::Run()
{
	// the update-script should not be automatically terminated when the program quits
	UnregisterRunningScript();

	m_prefixLen = 0;
	PrintMessage(Message::mkInfo, "Executing update-script %s", GetScript());

	BString<1024> infoName("update-script %s", FileSystem::BaseFileName(GetScript()));
	SetInfoName(infoName);

	const char* branchName[] = { "STABLE", "TESTING", "DEVEL" };
	SetEnvVar("NZBUP_BRANCH", branchName[m_branch]);

	SetEnvVar("NZBUP_RUNMODE", g_Options->GetDaemonMode() ? "DAEMON" : "SERVER");

	for (int i = 0; i < g_ArgumentCount; i++)
	{
		BString<100> envName("NZBUP_CMDLINE%i", i);
		SetEnvVar(envName, (*g_Arguments)[i]);
	}

#ifdef WIN32
	int pid = (int)GetCurrentProcessId();
#else
	int pid = (int)getpid();
#endif

	SetEnvVar("NZBUP_PROCESSID", BString<100>("%i", pid));

	BString<100> logPrefix = FileSystem::BaseFileName(GetScript());
	if (char* ext = strrchr(logPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(logPrefix);
	m_prefixLen = strlen(logPrefix) + 2; // 2 = strlen(": ");

	Execute();

	g_Maintenance->ResetUpdateController();
}

void UpdateScriptController::AddMessage(Message::EKind kind, const char* text)
{
	text = text + m_prefixLen;

	if (!strncmp(text, "[NZB] ", 6))
	{
		debug("Command %s detected", text + 6);
		if (!strcmp(text + 6, "QUIT"))
		{
			Detach();
			ExitProc();
		}
		else
		{
			error("Invalid command \"%s\" received", text);
		}
	}
	else
	{
		g_Maintenance->AddMessage(kind, Util::CurrentTime(), text);
		ScriptController::AddMessage(kind, text);
	}
}

void UpdateInfoScriptController::ExecuteScript(const char* script, CString& updateInfo)
{
	detail("Executing update-info-script %s", FileSystem::BaseFileName(script));

	UpdateInfoScriptController scriptController;
	scriptController.SetArgs({script});

	BString<1024> infoName("update-info-script %s", FileSystem::BaseFileName(script));
	scriptController.SetInfoName(infoName);

	BString<1024> logPrefix = FileSystem::BaseFileName(script);
	if (char* ext = strrchr(logPrefix, '.')) *ext = '\0'; // strip file extension
	scriptController.SetLogPrefix(logPrefix);
	scriptController.m_prefixLen = strlen(logPrefix) + 2; // 2 = strlen(": ");

	scriptController.Execute();

	if (!scriptController.m_updateInfo.Empty())
	{
		updateInfo = scriptController.m_updateInfo;
	}
}

void UpdateInfoScriptController::AddMessage(Message::EKind kind, const char* text)
{
	text = text + m_prefixLen;

	if (!strncmp(text, "[NZB] ", 6))
	{
		debug("Command %s detected", text + 6);
		if (!strncmp(text + 6, "[UPDATEINFO]", 12))
		{
			m_updateInfo.Append(text + 6 + 12);
		}
		else
		{
			error("Invalid command \"%s\" received from %s", text, GetInfoName());
		}
	}
	else
	{
		ScriptController::AddMessage(kind, text);
	}
}

#ifdef HAVE_OPENSSL
Signature::Signature(const char *inFilename, const char *sigFilename, const char *pubKeyFilename)
{
	m_inFilename = inFilename;
	m_sigFilename = sigFilename;
	m_pubKeyFilename = pubKeyFilename;
	m_sha256Ctx = nullptr;
	m_pubKeyCtx = nullptr;
}

Signature::~Signature()
{
    if ( m_sha256Ctx == nullptr )
    {
        EVP_MD_CTX_free( m_sha256Ctx );
    }
    if ( m_pubKeyCtx == nullptr )
    {
    	EVP_PKEY_CTX_free( m_pubKeyCtx );
    }
}

// Calculate SHA-256 for input file (m_inFilename)
bool Signature::ComputeSHA256()
{
	DiskFile infile;
	if (!infile.Open(m_inFilename, DiskFile::omRead))
	{
		return false;
	}
    if ( m_sha256Ctx == nullptr ) {
	    m_sha256Ctx = EVP_MD_CTX_new();
        if ( m_sha256Ctx == nullptr )
        {
            return false;
        }
    }
	EVP_DigestInit( m_sha256Ctx, EVP_sha256() );
	CharBuffer buffer(32*1024);
    size_t bytesRead;
	while ( ( bytesRead = infile.Read( buffer, buffer.Size() ) ) != 0 )
	{
		EVP_DigestUpdate( m_sha256Ctx, buffer, bytesRead);
	}
    // store the computed digest in m_sha256
	EVP_DigestFinal( m_sha256Ctx, m_sha256, &m_sha256Len );

	infile.Close();
	return true;
}

// Read signature from file (m_sigFilename) into memory
bool Signature::ReadSignature()
{
	BString<1024> sigTitle("\"RSA-SHA256(%s)\" : \"", FileSystem::BaseFileName(m_inFilename));

	DiskFile infile;
	if (!infile.Open(m_sigFilename, DiskFile::omRead))
	{
		return false;
	}

	bool ok = false;
	int titLen = strlen(sigTitle);
	char buf[1024];
	uchar* output = m_signature;
	while (infile.ReadLine(buf, sizeof(buf) - 1))
	{
		if (!strncmp(buf, sigTitle, titLen))
		{
			char* hexSig = buf + titLen;
			int sigLen = strlen(hexSig);
			if (sigLen > 2)
			{
				hexSig[sigLen - 2] = '\0'; // trim trailing ",
			}
			while (*hexSig && *(hexSig+1) && output != m_signature + sizeof(m_signature))
			{
				uchar c1 = *hexSig++;
				uchar c2 = *hexSig++;
				c1 = '0' <= c1 && c1 <= '9' ? c1 - '0' : 'A' <= c1 && c1 <= 'F' ? c1 - 'A' + 10 :
					'a' <= c1 && c1 <= 'f' ? c1 - 'a' + 10 : 0;
				c2 = '0' <= c2 && c2 <= '9' ? c2 - '0' : 'A' <= c2 && c2 <= 'F' ? c2 - 'A' + 10 :
					'a' <= c2 && c2 <= 'f' ? c2 - 'a' + 10 : 0;
				uchar ch = (c1 << 4) + c2;
				*output++ = (char)ch;
			}
			ok = ( output == ( m_signature + sizeof(m_signature) ) );

			break;
		}
	}

	infile.Close();
	return ok;
}

// Read public key from file (m_szPubKeyFilename) into memory
bool Signature::ReadPublicKey()
{
   	EVP_PKEY*       publicKey;

	CharBuffer keybuf;
	if (!FileSystem::LoadFileIntoBuffer(m_pubKeyFilename, keybuf, false))
	{
		return false;
	}
	BIO* mem = BIO_new_mem_buf(keybuf, keybuf.Size());
	publicKey = PEM_read_bio_PUBKEY( mem, nullptr, nullptr, nullptr );
	BIO_free(mem);

    if ( publicKey == nullptr )
    {
        return false;
    }
    m_pubKeyCtx = EVP_PKEY_CTX_new( publicKey, NULL /* no engine */ );
    EVP_PKEY_free( publicKey );

    if  ( m_pubKeyCtx == nullptr
     || ( EVP_PKEY_verify_init( m_pubKeyCtx) <= 0 )
     || ( EVP_PKEY_CTX_set_rsa_padding( m_pubKeyCtx, RSA_PKCS1_PADDING ) <= 0)
     || ( EVP_PKEY_CTX_set_signature_md( m_pubKeyCtx, EVP_sha256() ) <= 0) )
    {
        return false;
    }
	return true;
}

bool Signature::Verify()
{
    bool result;
    result = ReadPublicKey() && ReadSignature() && ComputeSHA256();
	if ( result == true ) {
        result = EVP_PKEY_verify( m_pubKeyCtx, m_signature, sizeof(m_signature), m_sha256, m_sha256Len );
    }
    return result;
}
#endif /* HAVE_OPENSSL */
