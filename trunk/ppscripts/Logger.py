#!/usr/bin/env python
#
# Logger post-processing script for NZBGet
#
# Copyright (C) 2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# $Revision$
# $Date$
#


##############################################################################
### NZBGET POST-PROCESSING SCRIPT                                          ###

# Save post-processing log into a file.
#
# This script saves post-processing log of nzb-file into file
# _postprocesslog.txt in the destination directory.
#
# NOTE: This script requires Python to be installed on your system.

### NZBGET POST-PROCESSING SCRIPT                                          ###
##############################################################################


import os
import sys
import datetime
from xmlrpclib import ServerProxy

# Exit codes used by NZBGet
POSTPROCESS_SUCCESS=93
POSTPROCESS_ERROR=94

# Check if the script is called from nzbget 11.0 or later
if not os.environ.has_key('NZBOP_SCRIPTDIR'):
	print('*** NZBGet post-processing script ***')
	print('This script is supposed to be called from nzbget (11.0 or later).')
	sys.exit(POSTPROCESS_ERROR)

# To get the post-processing log we connect to NZBGet via XML-RPC
# and call method "postqueue", which returns the list of post-processing job.
# The first item in the list is current job. This item has a field 'Log',
# containing an array of log-entries.
# For more info visit http://nzbget.sourceforge.net/RPC_API_reference

# First we need to know connection info: host, port and password of NZBGet server.
# NZBGet passes all configuration options to post-processing script as
# environment variables.
host = os.environ['NZBOP_CONTROLIP'];
port = os.environ['NZBOP_CONTROLPORT'];
password = os.environ['NZBOP_CONTROLPASSWORD'];

if host == '0.0.0.0': host = '127.0.0.1'

# Build an URL for XML-RPC requests
rpcUrl = 'http://nzbget:%s@%s:%s/xmlrpc' % (password, host, port);

# Create remote server object
server = ServerProxy(rpcUrl)

# Call remote method 'postqueue'. The only parameter tells how many log-entries to return as maximum.
postqueue = server.postqueue(10000)

# Get field 'Log' from the first post-processing job
log = postqueue[0]['Log']

# Now iterate through entries and save them to the output file
if len(log) > 0:
	f = open('%s/_postprocesslog.txt' % os.environ['NZBPP_DIRECTORY'], 'w')
	for entry in log:
		f.write('%s\t%s\t%s\n' % (entry['Kind'], datetime.datetime.fromtimestamp(int(entry['Time'])), entry['Text']))
	f.close()

# All OK, returning exit status 'POSTPROCESS_SUCCESS' (int <93>) to let NZBGet know
# that our script has successfully completed.
sys.exit(POSTPROCESS_SUCCESS)
