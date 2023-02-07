NZBGet for Windows
==================

> This is a summary. For full documentation visit
http://nzbget.net/Installation_on_Windows


NZBGet can be used in application mode (with tray icon and an optional
DOS window) or as a service.

> **Note:** When you use the program for the first time you should
> start it at least once in application mode. This creates the
> necessary configuration file from a template.

Batch file
----------

> nzbget-command-shell.bat

Starts console window (DOS box) where you can execute remote commands to communicate with running NZBGet.

Service mode
=====================

First you need to install the service.

From NZBGet shell (batch file `nzbget-shell.bat`) use command:
> nzbget -install

To remove the service use command:
> nzbget -remove

To start service:
> net start NZBGet

To stop service:
> net stop NZBGet
