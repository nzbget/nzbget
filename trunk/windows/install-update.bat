@echo off

rem
rem  Batch file to update nzbget from web-interface
rem
rem  Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
rem
rem  This program is free software; you can redistribute it and/or modify
rem  it under the terms of the GNU General Public License as published by
rem  the Free Software Foundation; either version 2 of the License, or
rem  (at your option) any later version.
rem
rem  This program is distributed in the hope that it will be useful,
rem  but WITHOUT ANY WARRANTY; without even the implied warranty of
rem  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem  GNU General Public License for more details.
rem
rem  You should have received a copy of the GNU General Public License
rem  along with this program; if not, write to the Free Software
rem  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
rem

title Updating NZBGet

set BASE_URL=http://sourceforge.net/projects/nzbget/files

if x%NZBUP_BRANCH%==x (
	echo This script is executed by NZBGet during update and is not supposed to be started manually by user.
	echo.
	echo.To update NZBGet go to Web-interface - Settings - System - Check for updates.
	echo.
	pause
	exit
)

@setlocal enabledelayedexpansion

rem extracting link to update-info-URL from "webui\package-info.json"
set UPDATE_INFO_LINK=
for /f "delims=" %%a in ('type "%NZBOP_WEBDIR%\package-info.json"') do (
	set line=%%a
	set line=!line:update-info-link=!
	if not %%a==!line! (
		set UPDATE_INFO_LINK=!line!
		rem deleting tabs, spaces, quotation marks and commas
		set UPDATE_INFO_LINK=!UPDATE_INFO_LINK:	=!
		set UPDATE_INFO_LINK=!UPDATE_INFO_LINK: =!
		set UPDATE_INFO_LINK=!UPDATE_INFO_LINK:"=!
		set UPDATE_INFO_LINK=!UPDATE_INFO_LINK:,=!
		rem deleteing the leading colon
		set UPDATE_INFO_LINK=!UPDATE_INFO_LINK:~1%!
	)
)

rem "%~dp0" means the location of the current batch file
set NZBGET_DIR=%~dp0
cd %NZBGET_DIR%

if "%1"=="/step2" goto STEP2

rem Determine if NZBGet is running as a service
set NZBGET_SERVICE=no
for /F "tokens=3 delims=: " %%H in ('sc query "NZBGet" ^| findstr "        STATE"') do (
	if /I "%%H" EQU "RUNNING" (
		set NZBGET_SERVICE=yes
	)
)

echo Downloading version information...
rem using special command "-B webget" NZBGet works like a simple wget
rem and fetches files from web-servers
nzbget.exe -B webget "%TEMP%\NZBGET_UPDATE.txt" "%UPDATE_INFO_LINK%"
if errorlevel 1 goto DOWNLOAD_FAILURE

if %NZBUP_BRANCH%==TESTING set VER_FIELD=testing-version
if %NZBUP_BRANCH%==STABLE set VER_FIELD=stable-version
set VER=0
for /f "delims=" %%a in (%TEMP%\NZBGET_UPDATE.txt) do (
	set line=%%a
	set line=!line:%VER_FIELD%=!
	if not %%a==!line! (
		set VER=!line!
		rem deleting tabs, spaces, quotation marks, colons and commas
		set VER=!VER:	=!
		set VER=!VER: =!
		set VER=!VER:"=!
		set VER=!VER::=!
		set VER=!VER:,=!
	)
)

SET SETUP_EXE=nzbget-%VER%-bin-win32-setup.exe

echo Downloading %SETUP_EXE%...
nzbget.exe -B webget "%TEMP%\%SETUP_EXE%" "%BASE_URL%/%SETUP_EXE%"
if errorlevel 1 goto DOWNLOAD_FAILURE
echo Downloaded successfully
rem using ping as wait-command, the third parameter (2) causes ping to wait 1 (one) second
ping 127.0.0.1 -n 2 -w 1000 > nul

echo Stopping NZBGet and installing update...
ping 127.0.0.1 -n 2 -w 1000 > nul

rem After NZBGet is stopped the script cannot pring any messages to web-interface
rem In order for user to see any error messages we start another instance of the
rem script with its own a console window.
rem We need to do that because of another reeson too. When the update is installed
rem it is possible that the script "install-update.bat" will be updated too.
rem In that case the command interpreter will go grazy because it doesn't like the
rem batch files being replaced during execution.
copy install-update.bat "%TEMP%\nzbget-update.bat" > nul
if errorlevel 1 goto COPYSCRIPT_FAILURE
start "Updating NZBGet" /I /MIN CALL "%TEMP%\nzbget-update.bat" /step2 "%NZBGET_DIR%" %SETUP_EXE% %NZBGET_SERVICE%

echo [NZB] QUIT

exit


:STEP2
rem init from command line params
set NZBGET_DIR=%2
cd %NZBGET_DIR%
set SETUP_EXE=%3
set NZBGET_SERVICE=%4

rem check if nzbget.exe is running
echo Stopping NZBGet...
echo.

tasklist 2> nul > nul
if errorlevel 1 goto WINXPHOME

set WAIT_SECONDS=30
:CHECK_RUNNING
if "%WAIT_SECONDS%"=="0" goto QUIT_FAILURE
tasklist /FI "IMAGENAME eq nzbget.exe" 2> nul | find /I /N "nzbget.exe" > nul
if "%ERRORLEVEL%"=="0" (
	ping 127.0.0.1 -n 2 -w 1000 > nul
	set /a "WAIT_SECONDS=%WAIT_SECONDS%-1"
	goto CHECK_RUNNING
)

goto INSTALL

:WINXPHOME
rem Alternative solution when command "tasklist" isn't available:
rem just wait 30 seconds
ping 127.0.0.1 -n 31 -w 1000 > nul

:INSTALL

echo Installing new version...
echo.
%TEMP%\%SETUP_EXE% /S

del %TEMP%\%SETUP_EXE%

echo Starting NZBGet...

if "%NZBGET_SERVICE%"=="yes" (
	net start NZBGet
) else (
	start /MIN nzbget.exe -app -auto -s
)
if errorlevel 1 goto START_FAILURE
ping 127.0.0.1 -n 2 -w 1000 > nul
exit


:DOWNLOAD_FAILURE
rem This is in the first instance, the error is printed to web-interface
echo.
echo [ERROR] ***********************************************
echo [ERROR] Download failed, please try again later
echo [ERROR] ***********************************************
echo.
exit


:COPYSCRIPT_FAILURE
rem This is in the first instance, the error is printed to web-interface
echo.
echo [ERROR] ***********************************************
echo [ERROR] Failed to copy the update script
echo [ERROR] ***********************************************
echo.
exit


:QUIT_FAILURE
rem This is in the second instance, the error is printed to console window
start "Error during update" CMD /c "echo ERROR: Failed to stop NZBGet && pause"
ping 127.0.0.1 -n 11 -w 1000 > nul
exit


:START_FAILURE
rem This is in the second instance, the error is printed to console window
start "Error during update" CMD /c "echo ERROR: Failed to start NZBGet && pause"
ping 127.0.0.1 -n 11 -w 1000 > nul
exit

