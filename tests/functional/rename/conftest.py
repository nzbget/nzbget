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

	if not os.path.exists(nserv_datadir + '/rarrename3'):
		os.makedirs(nserv_datadir + '/rarrename3')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part01.rar', nserv_datadir + '/rarrename3/abc.21')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part02.rar', nserv_datadir + '/rarrename3/abc.02')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part03.rar', nserv_datadir + '/rarrename3/abc.15')
		os.chdir(nserv_datadir + '/rarrename3')
		if 0 != subprocess.call([par2_bin, 'c', '-b20', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/rarrename5'):
		os.makedirs(nserv_datadir + '/rarrename5')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part01.rar', nserv_datadir + '/rarrename5/abc.21')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part02.rar', nserv_datadir + '/rarrename5/abc.02')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part03.rar', nserv_datadir + '/rarrename5/abc.15')
		os.chdir(nserv_datadir + '/rarrename5')
		if 0 != subprocess.call([par2_bin, 'c', '-b20', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/rarrename2sets'):
		os.makedirs(nserv_datadir + '/rarrename2sets')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part01.rar', nserv_datadir + '/rarrename2sets/abc.21')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part02.rar', nserv_datadir + '/rarrename2sets/abc.02')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part03.rar', nserv_datadir + '/rarrename2sets/abc.15')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part01.rar', nserv_datadir + '/rarrename2sets/abc.22')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part02.rar', nserv_datadir + '/rarrename2sets/abc.03')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile5.part03.rar', nserv_datadir + '/rarrename2sets/abc.14')

	if not os.path.exists(nserv_datadir + '/rarrename3on'):
		os.makedirs(nserv_datadir + '/rarrename3on')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.rar', nserv_datadir + '/rarrename3on/abc.61')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.r00', nserv_datadir + '/rarrename3on/abc.32')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.r01', nserv_datadir + '/rarrename3on/abc.45')

	if not os.path.exists(nserv_datadir + '/rarrename3badext'):
		os.makedirs(nserv_datadir + '/rarrename3badext')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.rar', nserv_datadir + '/rarrename3badext/testfile3on.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.r00', nserv_datadir + '/rarrename3badext/testfile3on.r03')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.r01', nserv_datadir + '/rarrename3badext/testfile3on.r02')

	if not os.path.exists(nserv_datadir + '/rarrename5badext'):
		os.makedirs(nserv_datadir + '/rarrename5badext')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part01.rar', nserv_datadir + '/rarrename5badext/testfile3.part01.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part02.rar', nserv_datadir + '/rarrename5badext/testfile3.part0002.rar')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3.part03.rar', nserv_datadir + '/rarrename5badext/testfile3.part03.rar')

	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '3000', '-q']):
		pytest.exit('Test file generation failed')

	if not os.path.exists(nserv_datadir + '/rarrename3sm'):
		os.makedirs(nserv_datadir + '/rarrename3sm')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.rar', nserv_datadir + '/rarrename3sm/abc.61')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.r00', nserv_datadir + '/rarrename3sm/abc.32')
		shutil.copyfile(testdata_dir + '/rarrenamer/testfile3on.r01', nserv_datadir + '/rarrename3sm/abc.45')
		os.chdir(nserv_datadir + '/rarrename3sm')
		if 0 != subprocess.call([par2_bin, 'c', '-b100', 'parrename.par2', '*']):
			pytest.exit('Test file generation failed')
	if 0 != subprocess.call([nzbget_bin, '--nserv', '-d', nserv_datadir, '-v', '2', '-z', '500', '-q']):
		pytest.exit('Test file generation failed')
