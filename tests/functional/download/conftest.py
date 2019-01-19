import os
import shutil
import subprocess
import pytest


def pytest_addoption(parser):
	parser.addini('sample_medium', 'size of meidum nzb (megabytes)', default=192)
	parser.addini('sample_large', 'size of large nzb (megabytes)', default=1024)


@pytest.fixture(scope='session', autouse=True)
def prepare_testdata(request):
	print('Preparing test data for "download"')
	pytest.check_config()

	nserv_datadir = pytest.config.getini('nserv_datadir')
	nzbget_bin = pytest.config.getini('nzbget_bin')
	sevenzip_bin = pytest.config.getini('sevenzip_bin')

	if not os.path.exists(nserv_datadir):
		print('Creating nserv datadir')
		os.makedirs(nserv_datadir)

	if not os.path.exists(nserv_datadir + '/medium.nzb'):
		sizemb = int(pytest.config.getini('sample_medium'))
		create_test_file(nserv_datadir + '/medium', sevenzip_bin, sizemb, 50)

	if not os.path.exists(nserv_datadir + '/large.nzb'):
		sizemb = int(pytest.config.getini('sample_large'))
		create_test_file(nserv_datadir + '/large', sevenzip_bin, sizemb, 50)

	if not os.path.exists(nserv_datadir + '/medium.nzb') or not os.path.exists(nserv_datadir + '/large.nzb'):
		if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '500000', '-q']):
			pytest.exit('Test file generation failed')

	nzbget_srcdir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
	if not os.path.exists(nserv_datadir + '/small.nzb'):
		if not os.path.exists(nserv_datadir + '/small'):
			os.makedirs(nserv_datadir + '/small')
		shutil.copyfile(nzbget_srcdir +'/COPYING', nserv_datadir + '/small/small.dat')

	if not os.path.exists(nserv_datadir + '/small-obfuscated.nzb'):
		if not os.path.exists(nserv_datadir + '/small-obfuscated'):
			os.makedirs(nserv_datadir + '/small-obfuscated')
		shutil.copyfile(nzbget_srcdir +'/COPYING', nserv_datadir + '/small-obfuscated/fsdkhKHGuwuMNBKskd')

	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '3000', '-q']):
		pytest.exit('Test file generation failed')


def create_test_file(bigdir, sevenzip_bin, sizemb, partmb):
	print('Preparing test file (' + str(sizemb) + 'MB)')

	if not os.path.exists(bigdir):
		os.makedirs(bigdir)

	f = open(bigdir + '/' + str(sizemb) + 'mb.dat', 'wb')
	for n in xrange(sizemb // partmb):
		print('Writing block %i from %i' % (n + 1, sizemb // partmb))
		f.write(os.urandom(partmb * 1024 * 1024))
	f.close()

	if 0 != subprocess.call([sevenzip_bin, 'a', bigdir + '/' + str(sizemb) + 'mb.7z', '-mx=0', '-v' + str(partmb) + 'm', bigdir + '/' + str(sizemb) + 'mb.dat']):
		pytest.exit('Test file generation failed')

	os.remove(bigdir + '/' + str(sizemb) + 'mb.dat')
