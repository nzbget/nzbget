import os
import shutil
import subprocess
import pytest

def pytest_addoption(parser):
	parser.addini('par2_bin', 'path to par2 binary', default=None)

@pytest.fixture(scope='session', autouse=True)
def prepare_testdata(request):
	print('Preparing test data for "rename"')

	nserv_datadir = pytest.config.getini('nserv_datadir')
	nzbget_bin = pytest.config.getini('nzbget_bin')
	par2_bin = pytest.config.getini('par2_bin')

	if not os.path.exists(par2_bin):
		pytest.exit('Cannot prepare test files. Set option "par2_bin in pytest.ini"')

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

	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '3000', '-q']):
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
