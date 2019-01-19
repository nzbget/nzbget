import os
import subprocess
import pytest


@pytest.fixture(scope='session', autouse=True)
def prepare_testdata(request):
	print('Preparing test data for "unpack"')

	nserv_datadir = pytest.config.getini('nserv_datadir')
	nzbget_bin = pytest.config.getini('nzbget_bin')
	sevenzip_bin = pytest.config.getini('sevenzip_bin')
	par2_bin = pytest.config.getini('par2_bin')

	if not os.path.exists(nserv_datadir):
		print('Creating nserv datadir')
		os.makedirs(nserv_datadir)

	if not os.path.exists(nserv_datadir + '/unpack-damaged.nzb'):
		create_test_file(nserv_datadir + '/unpack-damaged', sevenzip_bin, 3, 1)
		os.chdir(nserv_datadir + '/unpack-damaged')

		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'unpackcrc-damaged.par2', '*']):
			pytest.exit('Test file generation failed')

		outf = open("3mb.7z.001","rb+")
		outf.seek(100000)
		outf.write(b"\x0a\x1b\x2c")
		outf.close()

	if not os.path.exists(nserv_datadir + '/unpackcrc-par.nzb'):
		create_test_file(nserv_datadir + '/unpackcrc-par', sevenzip_bin, 3, 1)
		os.chdir(nserv_datadir + '/unpackcrc-par')

		outf = open("3mb.7z.001","rb+")
		outf.seek(100000)
		outf.write(b"\x0a\x1b\x2c")
		outf.close()

		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'unpackcrc-par.par2', '*']):
			pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/unpackcrc-nopar.nzb'):
		create_test_file(nserv_datadir + '/unpackcrc-nopar', sevenzip_bin, 3, 1)
		os.chdir(nserv_datadir + '/unpackcrc-nopar')

		outf = open("3mb.7z.001","rb+")
		outf.seek(100000)
		outf.write(b"\x0a\x1b\x2c")
		outf.close()

	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '3000', '-q']):
		pytest.exit('Test file generation failed')


def create_test_file(bigdir, sevenzip_bin, sizemb, partmb):
	print('Preparing test file (' + str(sizemb) + 'MB)')

	if not os.path.exists(bigdir):
		os.makedirs(bigdir)

	f = open(bigdir + '/' + str(sizemb) + 'mb.dat', 'wb')
	for n in xrange(sizemb / partmb):
		print('Writing block %i from %i' % (n + 1, sizemb / partmb))
		f.write(os.urandom(partmb * 1024 * 1024))
	f.close()

	if 0 != subprocess.call([sevenzip_bin, 'a', bigdir + '/' + str(sizemb) + 'mb.7z', '-mx=0', '-v' + str(partmb) + 'm', bigdir + '/' + str(sizemb) + 'mb.dat']):
		pytest.exit('Test file generation failed')

	os.remove(bigdir + '/' + str(sizemb) + 'mb.dat')


def test_unpack_repair(nserv, nzbget):
	hist = nzbget.download_nzb('unpack-damaged.nzb', unpack = True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_unpack_crcerror_par(nserv, nzbget):
	hist = nzbget.download_nzb('unpackcrc-par.nzb', unpack = True)
	assert hist['Status'] == 'FAILURE/UNPACK'

def test_unpack_crcerror_nopar(nserv, nzbget):
	hist = nzbget.download_nzb('unpackcrc-nopar.nzb', unpack = True)
	assert hist['Status'] == 'FAILURE/UNPACK'
