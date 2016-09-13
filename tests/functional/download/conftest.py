import os
import shutil
import subprocess
import random
import array
import pytest


@pytest.fixture(scope='session', autouse=True)
def prepare_testdata(request):
	print('Preparing test data for "download"')

	nserv_datadir = pytest.config.getini('nserv_datadir')
	nzbget_bin = pytest.config.getini('nzbget_bin')
	sevenzip_bin = pytest.config.getini('sevenzip_bin')

	if not os.path.exists(nserv_datadir):
		print('Creating nserv datadir')
		os.makedirs(nserv_datadir)

	if not os.path.exists(nserv_datadir + '/1gb.nzb'):
		create_gb(nserv_datadir + '/1gb', sevenzip_bin, 1)

	if not os.path.exists(nserv_datadir + '/9gb.nzb'):
		create_gb(nserv_datadir + '/9gb', sevenzip_bin, 9)

	if not os.path.exists(nserv_datadir + '/1gb.nzb') or not os.path.exists(nserv_datadir + '/9gb.nzb'):
		if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '500000', '-q']):
			pytest.exit('Test file generation failed')

	nzbget_srcdir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
	if not os.path.exists(nserv_datadir + '/1k.dat'):
		shutil.copyfile(nzbget_srcdir +'/COPYING', nserv_datadir + '/1k.dat')

	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '3000', '-q']):
		pytest.exit('Test file generation failed')


def create_gb(bigdir, sevenzip_bin, sizegb):
	print('Preparing large test file (' + str(sizegb) + 'GB)')

	if not os.path.exists(bigdir):
		os.makedirs(bigdir)

	f = open(bigdir + '/' + str(sizegb) + 'gb.dat', 'wb')
	for n in xrange(64 * sizegb):
		if n % 8 == 0:
			print('Writing block %i from %i' % (n, 64 * sizegb))
		f.write(os.urandom(1024 * 1024 * 16))
	f.close()

	if 0 != subprocess.call([sevenzip_bin, 'a', bigdir + '/' + str(sizegb) + 'gb.7z', '-sdel', '-mx=0', '-v50m', bigdir + '/' + str(sizegb) + 'gb.dat']):
		pytest.exit('Test file generation failed')
