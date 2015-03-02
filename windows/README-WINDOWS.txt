=====================================
      NZBGet ReadMe for Windows
=====================================

This is a short documentation. For more info visit
http://nzbget.net/Installation_on_Windows

=====================================

NZBGet can be used in application mode (tray icon and optional
DOS window) or as a service. When you use the program for the
first time you should start it at least once in application mode
to create the necessary configuration file from template.


=====================
Provided batch-file:
=====================

nzbget-command-shell.bat
Starts console window (DOS box) where you can execute remote
commands to communicate with running NZBGet.


=====================
Service mode
=====================

First you need to install the service.
From NZBGet shell (batch file nzbget-shell.bat) use command:
  nzbget -install

To remove the service use command:
  nzbget -remove

To start service:
  net start NZBGet

To stop service:
  net stop NZBGet

===================================================================
For description of the program and more information see file README.
