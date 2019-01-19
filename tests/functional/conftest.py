import pytest
import subprocess
import os
import time
import shutil
import base64
import distutils.spawn
try:
	from xmlrpclib import ServerProxy # python 2
except ImportError:
	from xmlrpc.client import ServerProxy # python 3

nzbget_srcdir = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
nzbget_maindir = nzbget_srcdir + '/tests/testdata/nzbget.temp'
nzbget_configfile = nzbget_maindir + '/nzbget.conf'
nzbget_rpcurl = 'http://127.0.0.1:6789/xmlrpc';

exe_ext = '.exe' if os.name == 'nt' else ''

nzbget_bin = nzbget_srcdir + '/nzbget' + exe_ext
nserv_datadir = nzbget_srcdir + '/tests/testdata/nserv.temp'

sevenzip_bin = distutils.spawn.find_executable('7z')
if sevenzip_bin is None:
	sevenzip_bin = nzbget_srcdir + '/7z' + exe_ext

par2_bin = distutils.spawn.find_executable('par2')
if par2_bin is None:
	par2_bin = nzbget_srcdir + '/par2' + exe_ext

has_failures = False

def pytest_addoption(parser):
	parser.addini('nzbget_bin', 'path to nzbget binary', default=nzbget_bin)
	parser.addini('nserv_datadir', 'path to nserv datadir', default=nserv_datadir)
	parser.addini('nzbget_maindir', 'path to nzbget maindir', default=nzbget_maindir)
	parser.addini('sevenzip_bin', 'path to 7-zip', default=sevenzip_bin)
	parser.addini('par2_bin', 'path to par2 binary', default=par2_bin)
	parser.addoption("--hold", action="store_true", help="Hold at the end of test (keep NZBGet running)")

def check_config():
	global nzbget_bin
	nzbget_bin = pytest.config.getini('nzbget_bin')
	if not os.path.exists(nzbget_bin):
		pytest.exit('Could not find nzbget binary at ' + nzbget_bin + '. Alternative path can be set via pytest ini option "nzbget_bin".')

	global sevenzip_bin, par2_bin
	sevenzip_bin = pytest.config.getini('sevenzip_bin')
	par2_bin = pytest.config.getini('par2_bin')
	if not os.path.exists(sevenzip_bin):
		pytest.exit('Could not find 7-zip binary in search path or at ' + sevenzip_bin + '. Alternative path can be set via pytest ini option "sevenzip_bin".')
	if not os.path.exists(par2_bin):
		pytest.exit('Could not find par2 binary in search path or at ' + par2_bin + '. Alternative path can be set via pytest ini option "par2_bin".')

	global nserv_datadir
	nserv_datadir = pytest.config.getini('nserv_datadir')

	global nzbget_maindir
	nzbget_maindir = pytest.config.getini('nzbget_maindir')
	global nzbget_configfile
	nzbget_configfile = nzbget_maindir + '/nzbget.conf'

pytest.check_config = check_config

class NServ:

	def __init__(self):
		self.process = subprocess.Popen([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '0', '-i', '2'])

	def finalize(self):
		self.process.kill()

@pytest.fixture(scope='session')

def nserv(request):
	check_config()

	instance = NServ()
	request.addfinalizer(instance.finalize)
	return instance


class Nzbget:

	def __init__(self, options, session):
		self.options = options
		self.session = session
		self.api = ServerProxy(nzbget_rpcurl)
		self.prepare_session()
		self.process = subprocess.Popen([nzbget_bin, '-c', nzbget_configfile, '-s', '-o', 'outputmode=log'])
		self.wait_until_started()

	def finalize(self):
		if pytest.config.getoption("--hold"):
			print('\nNZBGet is still running, press Ctrl+C to quit')
			time.sleep(100000)
		self.process.kill()
		if not has_failures:
			self.remove_tempdir()

	def remove_tempdir(self):
		attempt = 1
		completed = False
		while not completed:
			try:
				if os.path.exists(nzbget_maindir + '.old'):
					shutil.rmtree(nzbget_maindir + '.old')
				if os.path.exists(nzbget_maindir):
					os.rename(nzbget_maindir, nzbget_maindir + '.old')
					shutil.rmtree(nzbget_maindir + '.old')
				completed = True
			except Exception:
				if attempt > 20:
					raise
				attempt += 1
				time.sleep(0.2)

	def prepare_session(self):
		self.remove_tempdir()
		os.makedirs(nzbget_maindir)
		config = open(nzbget_configfile, 'w')
		config.write('MainDir=' + nzbget_maindir + '\n')
		config.write('DestDir=${MainDir}/complete\n')
		config.write('InterDir=${MainDir}/intermediate\n')
		config.write('TempDir=${MainDir}/temp\n')
		config.write('QueueDir=${MainDir}/queue\n')
		config.write('NzbDir=${MainDir}/nzb\n')
		config.write('LogFile=${MainDir}/nzbget.log\n')
		config.write('SevenZipCmd=' + sevenzip_bin + '\n')
		config.write('WriteLog=append\n')
		config.write('DetailTarget=log\n')
		config.write('InfoTarget=log\n')
		config.write('WarningTarget=log\n')
		config.write('ErrorTarget=log\n')
		config.write('DebugTarget=none\n')
		config.write('CrashTrace=no\n')
		config.write('CrashDump=yes\n')
		config.write('ContinuePartial=no\n')
		config.write('DirectWrite=yes\n')
		config.write('ArticleCache=500\n')
		config.write('WriteBuffer=1024\n')
		config.write('NzbDirInterval=0\n')
		config.write('FlushQueue=no\n')
		config.write('WebDir=' + nzbget_srcdir + '/webui\n')
		config.write('ConfigTemplate=' + nzbget_srcdir + '/nzbget.conf\n')
		config.write('ControlUsername=\n')
		config.write('ControlPassword=\n')
		config.write('ControlIP=127.0.0.1\n')
		config.write('Server1.host=127.0.0.1\n')
		config.write('Server1.port=6791\n')
		config.write('Server1.connections=10\n')
		config.write('Server1.level=0\n')
		config.write('Server2.host=127.0.0.1\n')
		config.write('Server2.port=6792\n')
		config.write('Server2.connections=10\n')
		config.write('Server2.level=1\n')
		config.write('Server2.active=no\n')
		for opt in self.options:
			config.write(opt + '\n')

	def wait_until_started(self):
		print('Waiting for nzbget to start')
		stat = None
		for x in range(0, 3):
			try:
				stat = self.api.status()
			except Exception:
				time.sleep(0.5)

		if stat is None:
			raise Exception('Could not start nzbget')
		print('Started')

	def append_nzb(self, nzb_name, nzb_content, unpack = None, dupekey = '', dupescore = 0, dupemode = 'FORCE', params = None):
		nzbcontent64 = base64.standard_b64encode(nzb_content)
		if params is None:
			params = []
		if unpack == True:
			params.append(('*unpack:', 'yes'))
		elif unpack == False:
			params.append(('*unpack:', 'no'))
		return self.api.append(nzb_name, nzbcontent64, 'test', 0, False, False, dupekey, dupescore, dupemode, params)

	def load_nzb(self, nzb_name):
		fullfilename = nserv_datadir + '/' + nzb_name
		in_file = open(fullfilename, 'r')

		nzbcontent = in_file.read()

		in_file.close()
		return nzbcontent

	def download_nzb(self, nzb_name, nzb_content = None, unpack = None, dupekey = '', dupescore = 0, dupemode = 'FORCE', params = None):
		if not nzb_content:
			nzb_content = self.load_nzb(nzb_name)
		self.append_nzb(nzb_name, nzb_content, unpack, dupekey, dupescore, dupemode, params)
		hist = self.wait_nzb(nzb_name)
		return hist

	def wait_nzb(self, nzb_name):
		print('Waiting for download completion')
		hist = None
		while not hist:
			history = self.api.history()
			for hist1 in history:
				if hist1['NZBFilename'] == nzb_name:
					hist = hist1
					break
			time.sleep(0.1)
		return hist

	def clear(self):
		self.api.editqueue('HistoryFinalDelete', 0, '', range(1, 1000));

@pytest.fixture(scope='module')

def nzbget(request):
	check_config()

	instance = Nzbget(getattr(request.module, 'nzbget_options', []), request.session)
	request.addfinalizer(instance.finalize)
	return instance


@pytest.hookimpl(tryfirst=True, hookwrapper=True)

def pytest_runtest_makereport(item, call):

	# execute all other hooks to obtain the report object

	outcome = yield

	rep = outcome.get_result()
	global has_failures

	has_failures = has_failures or rep.failed