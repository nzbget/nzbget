/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2014-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

; This is setup script for NZBGet for Windows. To compile the script you need
; NSIS (http://nsis.sourceforge.net). Moreover a special build of NSIS must be
; installed over standard NSIS installation. This special build provides
; extra logging required by the install script:
;  - Speical build with extra logging - http://nsis.sourceforge.net/Special%5FBuilds
; The install script also requires additional plugins:
;  - NSIS Simple Service Plugin - http://nsis.sourceforge.net/NSIS_Simple_Service_Plugin
;  - AccessControl plug-in - http://nsis.sourceforge.net/AccessControl_plug-in


;--------------------------------
;Includes

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"

;--------------------------------
;General

Name "NZBGet"
OutFile "..\nzbget-setup.exe"

;Default installation folder
InstallDir "$PROGRAMFILES\NZBGet"

;Get installation folder from registry if available
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NZBGet" "InstallLocation"

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

!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION RunAction
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
!define MUI_PAGE_CUSTOMFUNCTION_SHOW MyFinishShow
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE MyFinishLeave
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

Delete "$INSTDIR\install.log"

; Command "LogSet" requires a special build of NSIS supporting extended logging:
; http://nsis.sourceforge.net/Special%5FBuilds
LogSet on

SetOutPath "$INSTDIR"

; Stop NZBGet (if running)
${If} ${FileExists} "$INSTDIR\nzbget.exe"
  Delete "$INSTDIR\nzbget.exe"
  ExecWait '"$INSTDIR\nzbget.exe" -Q' $R2
  DetailPrint "Stopping NZBGet..."

  try_delete:
  ; Wait up to 10 seconds until stopped
  StrCpy $R2 20
  ${While} ${FileExists} "$INSTDIR\nzbget.exe"
    ${If} $R2 = 0
      ${Break}
    ${EndIf}
    Sleep 500
    IntOp $R2 $R2 - 1
    Delete "$INSTDIR\nzbget.exe"
  ${EndWhile}

  ${If} ${FileExists} "$INSTDIR\nzbget.exe"
    MessageBox MB_RETRYCANCEL "NZBGet seems to be running right now. Please stop NZBGet and try again." \
      IDRETRY try_delete IDCANCEL cancel
    cancel:
      abort
  ${EndIf}
${EndIf}

!ifndef DEBUG_UI

File "..\NZBGet\*"
SetOutPath "$INSTDIR\webui"
File /r "..\NZBGet\webui\*"

${If} ${FileExists} "$INSTDIR\nzbget.conf"
  ; When updating a portable installation install all scripts into exe-directory
  SetOutPath "$INSTDIR\scripts"
${Else}
  ; In default mode install all scripts into app-data-directory
  SetShellVarContext all
  SetOutPath "$APPDATA\NZBGet\scripts"
  # Make directory "$APPDATA\NZBGet" full access by all users
  AccessControl::GrantOnFile "$APPDATA\NZBGet" "(BU)" "FullAccess"
  Pop $0
  SetShellVarContext current
${EndIf}
File "..\NZBGet\scripts\*"

!endif

; Create shortcuts
CreateDirectory "$SMPROGRAMS\NZBGet"
CreateShortCut "$SMPROGRAMS\NZBGet\NZBGet.lnk" "$INSTDIR\nzbget.exe"
CreateShortCut "$SMPROGRAMS\NZBGet\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

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


Function RunAction

${If} ${AtLeastWinVista}
  ; Starting NZBGet with standard user privileges
  Exec "runas /trustlevel:0x20000 $\"$INSTDIR\nzbget.exe$\""
${Else}
  ; Starting NZBGet with current privileges
  Exec "$INSTDIR\nzbget.exe"
${EndIf}

FunctionEnd


; Add an extra checkbox for file association
var Checkbox
var Checkbox_State

Function MyFinishShow
${NSD_CreateCheckbox} 120u 130u 100% 10u "Associate With NZB Files"
Pop $Checkbox
SetCtlColors $Checkbox "" "ffffff"
FunctionEnd

Function MyFinishLeave
${NSD_GetState} $Checkbox $Checkbox_State
${If} $Checkbox_State <> 0

	WriteRegStr HKCR ".nzb" "" "NZBGet.NZBFile"
	WriteRegStr HKCR "NZBGet.NZBFile" "" `NZB File`
	WriteRegStr HKCR "NZBGet.NZBFile\DefaultIcon" "" `$INSTDIR\nzbget.exe,00`
	WriteRegStr HKCR "NZBGet.NZBFile\shell" "" "open"
	WriteRegStr HKCR "NZBGet.NZBFile\shell\open" "" `Open with NZBGet`
	WriteRegStr HKCR "NZBGet.NZBFile\shell\open\command" "" `$INSTDIR\nzbget.exe -A $\"%1$\"`	
	
	System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'
	
${EndIf}
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

ReadRegStr $R0 HKCR ".nzb" ""
${If} $R0 == "NZBGet.NZBFile"
	DeleteRegKey HKCR `NZBGet.NZBFile`
	DeleteRegValue HKCR ".nzb" ""
${EndIf}

; Refresh desktop window
System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'

SectionEnd
