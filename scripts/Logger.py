#!/usr/bin/env python
#
# Logger post-processing script for NZBGet
#
# Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


##############################################################################
### NZBGET POST-PROCESSING SCRIPT                                          ###

# Save nzb log into a file.
#
# This script saves the download and post-processing log of nzb-file
# into file _nzblog.txt in the destination directory.
#
# NOTE: This script requires Python to be installed on your system.

### NZBGET POST-PROCESSING SCRIPT                                          ###
##############################################################################


import os
import sys
import datetime

try:
	from xmlrpclib import ServerProxy # python 2
	from urllib2 import quote
except ImportError:
	from xmlrpc.client import ServerProxy # python 3
	from urllib.parse import quote

# Exit codes used by NZBGet
POSTPROCESS_SUCCESS=93
POSTPROCESS_NONE=95
POSTPROCESS_ERROR=94

# Check if the script is called from nzbget 15.0 or later
if not 'NZBOP_NZBLOG' in os.environ:
	print('*** NZBGet post-processing script ***')
	print('This script is supposed to be called from nzbget (15.0 or later).')
	sys.exit(POSTPROCESS_ERROR)

if not os.path.exists(os.environ['NZBPP_DIRECTORY']):
	print('Destination directory doesn\'t exist, exiting')
	sys.exit(POSTPROCESS_NONE)

# To get the item log we connect to NZBGet via XML-RPC and call
# method "loadlog", which returns the log for a given nzb item.
# For more info visit http://nzbget.net/RPC_API_reference

# First we need to know connection info: host, port and password of NZBGet server.
# NZBGet passes all configuration options to post-processing script as
# environment variables.
host = os.environ['NZBOP_CONTROLIP'];
port = os.environ['NZBOP_CONTROLPORT'];
username = os.environ['NZBOP_CONTROLUSERNAME'];
password = os.environ['NZBOP_CONTROLPASSWORD'];

if host == '0.0.0.0': host = '127.0.0.1'

# Build a URL for XML-RPC requests
rpcUrl = 'http://%s:%s@%s:%s/xmlrpc' % (quote(username), quote(password), host, port);

# Create remote server object
server = ServerProxy(rpcUrl)

# Call remote method 'loadlog'
nzbid = int(os.environ['NZBPP_NZBID'])
log = server.loadlog(nzbid, 0, 10000)

# Now iterate through entries and save them to the output file
if len(log) > 0:
	f = open('%s/_nzblog.txt' % os.environ['NZBPP_DIRECTORY'], 'wb')
	for entry in log:
		f.write((u'%s\t%s\t%s\n' % (entry['Kind'], datetime.datetime.fromtimestamp(int(entry['Time'])), entry['Text'])).encode('utf8'))
	f.close()

# All OK, returning exit status 'POSTPROCESS_SUCCESS' (int <93>) to let NZBGet know
# that our script has successfully completed.
sys.exit(POSTPROCESS_SUCCESS)
