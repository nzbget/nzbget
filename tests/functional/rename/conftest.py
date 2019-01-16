import os
import shutil
import subprocess
import pytest

@pytest.fixture(scope='session', autouse=True)
def prepare_testdata(request):
	print('Preparing test data for "rename"')
	pytest.check_config()

	nserv_datadir = pytest.config.getini('nserv_datadir')
	nzbget_bin = pytest.config.getini('nzbget_bin')
	sevenzip_bin = pytest.config.getini('sevenzip_bin')
	par2_bin = pytest.config.getini('par2_bin')

	if not os.path.exists(nserv_datadir):
		print('Creating nserv datadir')
		os.makedirs(nserv_datadir)

	nzbget_srcdir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
	testdata_dir = nzbget_srcdir + '/tests/testdata'

	if not os.path.exists(nserv_datadir + '/parrename'):
		os.makedirs(nserv_datadir + '/parrename')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part01.rar', nserv_datadir + '/parrename/testfile3.part01.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part02.rar', nserv_datadir + '/parrename/testfile3.part02.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part03.rar', nserv_datadir + '/parrename/testfile3.part03.rar')
		os.chdir(nserv_datadir + '/parrename')
		if 0 != subprocess.call([par2_bin, 'c', '-b20', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')
		os.rename(nserv_datadir + '/parrename/testfile3.part01.rar', nserv_datadir + '/parrename/abc.21')
		os.rename(nserv_datadir + '/parrename/testfile3.part02.rar', nserv_datadir + '/parrename/abc.02')
		os.rename(nserv_datadir + '/parrename/testfile3.part03.rar', nserv_datadir + '/parrename/abc.15')

	def prepare_test(dirname, testfile):
		if not os.path.exists(nserv_datadir + '/' + dirname + '.nzb'):
			os.makedirs(nserv_datadir + '/' + dirname)
			shutil.copyfile(testdata_dir + '/rarrenamer/' + testfile + '.part01.rar', nserv_datadir + '/' + dirname + '/abc.21')
			shutil.copyfile(testdata_dir + '/rarrenamer/' + testfile + '.part02.rar', nserv_datadir + '/' + dirname + '/abc.02')
			shutil.copyfile(testdata_dir + '/rarrenamer/' + testfile + '.part03.rar', nserv_datadir + '/' + dirname + '/abc.15')
			os.chdir(nserv_datadir + '/' + dirname)
			if 0 != subprocess.call([par2_bin, 'c', '-b20', 'parrename.par2', '*']):
				pytest.exit('Test file generation failed')

	prepare_test('rarrename3', 'testfile3')
	prepare_test('rarrename5', 'testfile5')
	prepare_test('rarrename3encdata', 'testfile3encdata')
	prepare_test('rarrename5encdata', 'testfile5encdata')
	prepare_test('rarrename3encnam', 'testfile3encnam')
	prepare_test('rarrename5encnam', 'testfile5encnam')

	if not os.path.exists(nserv_datadir + '/rarrename2sets'):
		os.makedirs(nserv_datadir + '/rarrename2sets')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part01.rar', nserv_datadir + '/rarrename2sets/abc.21')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part02.rar', nserv_datadir + '/rarrename2sets/abc.02')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part03.rar', nserv_datadir + '/rarrename2sets/abc.15')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part01.rar', nserv_datadir + '/rarrename2sets/abc.22')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part02.rar', nserv_datadir + '/rarrename2sets/abc.03')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part03.rar', nserv_datadir + '/rarrename2sets/abc.14')

	if not os.path.exists(nserv_datadir + '/rarrename3oldnam'):
		os.makedirs(nserv_datadir + '/rarrename3oldnam')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.rar', nserv_datadir + '/rarrename3oldnam/abc.61')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.r00', nserv_datadir + '/rarrename3oldnam/abc.32')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.r01', nserv_datadir + '/rarrename3oldnam/abc.45')

	if not os.path.exists(nserv_datadir + '/rarrename3badext'):
		os.makedirs(nserv_datadir + '/rarrename3badext')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.rar', nserv_datadir + '/rarrename3badext/testfile3oldnam.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.r00', nserv_datadir + '/rarrename3badext/testfile3oldnam.r03')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.r01', nserv_datadir + '/rarrename3badext/testfile3oldnam.r02')

	if not os.path.exists(nserv_datadir + '/rarrename5badext'):
		os.makedirs(nserv_datadir + '/rarrename5badext')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part01.rar', nserv_datadir + '/rarrename5badext/testfile3.part01.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part02.rar', nserv_datadir + '/rarrename5badext/testfile3.part0002.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part03.rar', nserv_datadir + '/rarrename5badext/testfile3.part03.rar')

	if not os.path.exists(nserv_datadir + '/rar3ignoreext'):
		os.makedirs(nserv_datadir + '/rar3ignoreext')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part01.rar', nserv_datadir + '/rar3ignoreext/testfile3-1.cbr')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part02.rar', nserv_datadir + '/rar3ignoreext/testfile3-2.cbr')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part03.rar', nserv_datadir + '/rar3ignoreext/testfile3-3.cbr')

	if not os.path.exists(nserv_datadir + '/parjoin1.nzb'):
		os.makedirs(nserv_datadir + '/parjoin1')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.dat', nserv_datadir + '/parjoin1/testfile.dat')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.nfo', nserv_datadir + '/parjoin1/testfile.nfo')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.par2', nserv_datadir + '/parjoin1/testfile.par2')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.vol00+1.PAR2', nserv_datadir + '/parjoin1/testfile.vol00+1.PAR2')
		split_test_file(nserv_datadir + '/parjoin1', 'testfile.dat', 50244);

	if not os.path.exists(nserv_datadir + '/parjoin2.nzb'):
		os.makedirs(nserv_datadir + '/parjoin2')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.dat', nserv_datadir + '/parjoin2/testfile.dat')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.nfo', nserv_datadir + '/parjoin2/testfile.nfo')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.par2', nserv_datadir + '/parjoin2/testfile.par2')
		shutil.copyfile(testdata_dir + '/parchecker/testfile.vol00+1.PAR2', nserv_datadir + '/parjoin2/testfile.vol00+1.PAR2')
		split_test_file(nserv_datadir + '/parjoin2', 'testfile.dat', 50244);
		os.rename(nserv_datadir + '/parjoin2/testfile.dat.001', nserv_datadir + '/parjoin2/renamed.001')
		os.rename(nserv_datadir + '/parjoin2/testfile.dat.002', nserv_datadir + '/parjoin2/renamed.002')
		os.rename(nserv_datadir + '/parjoin2/testfile.dat.003', nserv_datadir + '/parjoin2/renamed.003')

	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '3000', '-q']):
		pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/parjoin3.nzb'):
		os.makedirs(nserv_datadir + '/parjoin3')
		create_test_file(nserv_datadir + '/parjoin3', None, 20, 1)
		os.chdir(nserv_datadir + '/parjoin3')
		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')
		split_test_file(nserv_datadir + '/parjoin3', '20mb.dat', 7*1024*1024);
		os.rename(nserv_datadir + '/parjoin3/20mb.dat.001', nserv_datadir + '/parjoin3/renamed.001')
		os.rename(nserv_datadir + '/parjoin3/20mb.dat.002', nserv_datadir + '/parjoin3/renamed.002')
		os.rename(nserv_datadir + '/parjoin3/20mb.dat.003', nserv_datadir + '/parjoin3/renamed.003')
		if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '100000', '-q']):
			pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/rarrename3sm'):
		os.makedirs(nserv_datadir + '/rarrename3sm')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.rar', nserv_datadir + '/rarrename3sm/abc.61')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.r00', nserv_datadir + '/rarrename3sm/abc.32')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3oldnam.r01', nserv_datadir + '/rarrename3sm/abc.45')
		os.chdir(nserv_datadir + '/rarrename3sm')
		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')
		if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '500', '-q']):
			pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/obfuscated1.nzb'):
		create_test_file(nserv_datadir + '/obfuscated1', sevenzip_bin, 5, 1)
		os.chdir(nserv_datadir + '/obfuscated1')
		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')
		os.rename(nserv_datadir + '/obfuscated1/5mb.7z.001', nserv_datadir + '/obfuscated1/abc.51')
		os.rename(nserv_datadir + '/obfuscated1/5mb.7z.002', nserv_datadir + '/obfuscated1/abc.01')
		os.rename(nserv_datadir + '/obfuscated1/5mb.7z.003', nserv_datadir + '/obfuscated1/abc.21')
		os.rename(nserv_datadir + '/obfuscated1/5mb.7z.004', nserv_datadir + '/obfuscated1/abc.34')
		os.rename(nserv_datadir + '/obfuscated1/5mb.7z.005', nserv_datadir + '/obfuscated1/abc.17')
		os.rename(nserv_datadir + '/obfuscated1/5mb.7z.006', nserv_datadir + '/obfuscated1/abc.00')
		if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '100000', '-q']):
			pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/obfuscated2.nzb'):
		create_test_file(nserv_datadir + '/obfuscated2', sevenzip_bin, 5, 1)
		os.chdir(nserv_datadir + '/obfuscated2')
		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')
		os.rename(nserv_datadir + '/obfuscated2/5mb.7z.001', nserv_datadir + '/obfuscated2/abc.51')
		os.rename(nserv_datadir + '/obfuscated2/5mb.7z.002', nserv_datadir + '/obfuscated2/abc.01')
		os.rename(nserv_datadir + '/obfuscated2/5mb.7z.003', nserv_datadir + '/obfuscated2/abc.21')
		os.rename(nserv_datadir + '/obfuscated2/5mb.7z.004', nserv_datadir + '/obfuscated2/abc.34')
		os.rename(nserv_datadir + '/obfuscated2/5mb.7z.005', nserv_datadir + '/obfuscated2/abc.17')
		os.rename(nserv_datadir + '/obfuscated2/5mb.7z.006', nserv_datadir + '/obfuscated2/abc.00')
		os.rename(nserv_datadir + '/obfuscated2/parrename.par2', nserv_datadir + '/obfuscated2/abc.90')
		os.rename(nserv_datadir + '/obfuscated2/parrename.vol0+1.par2', nserv_datadir + '/obfuscated2/abc.95')
		os.rename(nserv_datadir + '/obfuscated2/parrename.vol1+2.par2', nserv_datadir + '/obfuscated2/abc.91')
		os.rename(nserv_datadir + '/obfuscated2/parrename.vol3+2.par2', nserv_datadir + '/obfuscated2/abc.92')
		if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '100000', '-q']):
			pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/obfuscated3.nzb'):
		create_test_file(nserv_datadir + '/obfuscated3', sevenzip_bin, 100, 10)
		os.chdir(nserv_datadir + '/obfuscated3')
		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.001', nserv_datadir + '/obfuscated3/abc.51')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.002', nserv_datadir + '/obfuscated3/abc.01')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.003', nserv_datadir + '/obfuscated3/abc.21')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.004', nserv_datadir + '/obfuscated3/abc.34')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.005', nserv_datadir + '/obfuscated3/abc.17')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.006', nserv_datadir + '/obfuscated3/abc.60')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.007', nserv_datadir + '/obfuscated3/abc.32')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.008', nserv_datadir + '/obfuscated3/abc.35')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.009', nserv_datadir + '/obfuscated3/abc.41')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.010', nserv_datadir + '/obfuscated3/abc.50')
		os.rename(nserv_datadir + '/obfuscated3/100mb.7z.011', nserv_datadir + '/obfuscated3/abc.43')
		os.rename(nserv_datadir + '/obfuscated3/parrename.par2', nserv_datadir + '/obfuscated3/abc.00')
		os.rename(nserv_datadir + '/obfuscated3/parrename.vol0+1.par2', nserv_datadir + '/obfuscated3/abc.02')
		os.rename(nserv_datadir + '/obfuscated3/parrename.vol1+2.par2', nserv_datadir + '/obfuscated3/abc.91')
		os.rename(nserv_datadir + '/obfuscated3/parrename.vol3+2.par2', nserv_datadir + '/obfuscated3/abc.92')
		if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '100000', '-q']):
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

	if sevenzip_bin != None:
		if 0 != subprocess.call([sevenzip_bin, 'a', bigdir + '/' + str(sizemb) + 'mb.7z', '-mx=0', '-v' + str(partmb) + 'm', bigdir + '/' + str(sizemb) + 'mb.dat']):
			pytest.exit('Test file generation failed')

		os.remove(bigdir + '/' + str(sizemb) + 'mb.dat')

def split_test_file(filedir, filename, partsize):
	print('Splitting test file ' + filename)

	inp = open(filedir + '/' + filename, 'rb')
	inp.seek(0,2) # move the cursor to the end of the file
	size = inp.tell()	
	inp.seek(0,0) # move the cursor to the start of the file

	written = 0
	part = 1
	while (written < size):
		block = inp.read(partsize)
		written += partsize
		f = open(filedir + '/' + filename + '.' + str(part).zfill(3), 'wb')
		f.write(block)
		f.close()
		part += 1
	inp.close()

	os.remove(filedir + '/' + filename)
