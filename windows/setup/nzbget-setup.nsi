/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2014-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


;--------------------------------
;Includes

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

; Also requires NSIS Simple Service Plugin:
; http://nsis.sourceforge.net/NSIS_Simple_Service_Plugin

;--------------------------------
;General

Name "NZBGet"
OutFile "..\nzbget-setup.exe"

;Default installation folder
InstallDir "$PROGRAMFILES\NZBGet"

;Get installation folder from registry if available
InstallDirRegKey HKCU "Software\NZBGet" ""

!ifndef DEBUG_UI
;Request application privileges for Windows Vista
RequestExecutionLevel admin
!endif

;--------------------------------
;Interface Settings

;  !define MUI_ABORTWARNING

!define MUI_ICON "..\resources\mainicon.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"

!define MUI_WELCOMEFINISHPAGE_BITMAP "install.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "uninstall.bmp"

!define MUI_FINISHPAGE_RUN "$INSTDIR\nzbget.exe"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Create Desktop Shortcut"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION FinishPageAction

;--------------------------------
;Pages

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\NZBGet\COPYING"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Main"

SetOutPath "$INSTDIR"

; Stop NZBGet (if running)
ReadRegStr $R1 HKCU "Software\NZBGet" ""
${If} $R1 != ""
${AndIf} ${FileExists} "$R1\nzbget.exe"
  Delete "$R1\nzbget.exe"
  ExecWait '"$R1\nzbget.exe" -Q' $R2
  DetailPrint "Stopping NZBGet..."

  try_delete:
  ; Wait up to 10 seconds until stopped
  StrCpy $R2 20
  ${While} ${FileExists} "$R1\nzbget.exe"
    ${If} $R2 = 0
      ${Break}
    ${EndIf}
    Sleep 500
    IntOp $R2 $R2 - 1
    Delete "$R1\nzbget.exe"
  ${EndWhile}

  ${If} ${FileExists} "$R1\nzbget.exe"
    MessageBox MB_RETRYCANCEL "NZBGet seems to be running right now. Please stop NZBGet and try again." \
      IDRETRY try_delete IDCANCEL cancel
    cancel:
      abort
  ${EndIf}
${EndIf}

!ifndef DEBUG_UI
File /r "..\NZBGet\*"
!endif

; Create shortcuts
CreateDirectory "$SMPROGRAMS\NZBGet"
CreateShortCut "$SMPROGRAMS\NZBGet\NZBGet.lnk" "$INSTDIR\nzbget.exe"
CreateShortCut "$SMPROGRAMS\NZBGet\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

; Store installation folder
WriteRegStr HKCU "Software\NZBGet" "" $INSTDIR

; Add control panel entry for Uninstall
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NZBGet" "DisplayName" "NZBGet"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NZBGet" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NZBGet" "Publisher" "Andrey Prygunkov"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NZBGet" "InstallLocation" "$INSTDIR"

${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
IntFmt $0 "0x%08X" $0
WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NZBGet" "EstimatedSize" "$0"

; Create uninstaller
WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd


Function FinishPageAction

; Create desktop shortcut
CreateShortcut "$DESKTOP\NZBGet.lnk" "$INSTDIR\nzbget.exe"

; Refresh desktop window
System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'

FunctionEnd


;--------------------------------
;Uninstaller Section

Section "Uninstall"

; Stop service (if installed)
SimpleSC::StopService "NZBGet" 1 30
Pop $0 ; returns an errorcode (<>0) otherwise success (0)

; Remove a service
SimpleSC::RemoveService "NZBGet"
Pop $0 ; returns an errorcode (<>0) otherwise success (0)

try_delete:
Delete "$INSTDIR\nzbget.exe"
IfFileExists "$INSTDIR\nzbget.exe" 0 not_running
MessageBox MB_RETRYCANCEL "File nzbget.exe could not be deleted. Please make sure the program isn't running." \
  IDRETRY try_delete IDCANCEL cancel
cancel:
  quit
not_running:

RMDir /r "$INSTDIR"
RMDir /r "$SMPROGRAMS\NZBGet"
Delete "$DESKTOP\NZBGet.lnk"

DeleteRegKey HKCU "Software\NZBGet"
DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NZBGet"

; Refresh desktop window
System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'

SectionEnd
