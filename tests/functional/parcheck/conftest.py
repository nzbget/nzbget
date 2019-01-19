import os
import shutil
import subprocess
import pytest


@pytest.fixture(scope='session', autouse=True)
def prepare_testdata(request):
	print('Preparing test data for "parcheck"')

	nserv_datadir = pytest.config.getini('nserv_datadir')
	nzbget_bin = pytest.config.getini('nzbget_bin')

	if not os.path.exists(nserv_datadir):
		print('Creating nserv datadir')
		os.makedirs(nserv_datadir)

	nzbget_srcdir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))

	testdata_dir = nzbget_srcdir + '/tests/testdata'
	if not os.path.exists(nserv_datadir + '/parchecker'):
		shutil.copytree(testdata_dir +'/parchecker', nserv_datadir + '/parchecker')
	if not os.path.exists(nserv_datadir + '/parchecker2'):
		shutil.copytree(testdata_dir +'/parchecker2', nserv_datadir + '/parchecker2')

	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '3000', '-q']):
		pytest.exit('Test file generation failed')
