@echo off

rem
rem  Batch file to start nzbget shell
rem
rem  Copyright (C) 2009 orbisvicis <orbisvicis@users.sourceforge.net>
rem  Copyright (C) 2009 Andrei Prygounkov <hugbug@users.sourceforge.net>
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
rem  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
rem

rem #######################    Usage instructions     #######################
rem 
rem  After starting the batch file you can use all nzbget commands 
rem  (like nzbget -s, nzbget -L, etc) without typing the full
rem  path to nzbget executable.
rem
rem ####################### End of Usage instructions #######################


rem expression "%~dp0" means the location of an executing batch file
set PATH=%PATH%;%~dp0
cmd /U /K "cd %USERPROFILE% & nzbget"
