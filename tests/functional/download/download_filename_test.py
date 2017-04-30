import os

nzbget_options = ['HealthCheck=none', 'ArticleCache=500', 'DirectWrite=yes', 'FileNaming=auto']

def test_small_obfuscated(nserv, nzbget):
	hist = nzbget.download_nzb('small-obfuscated.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'
	assert os.path.exists(hist['DestDir'] + '/fsdkhKHGuwuMNBKskd')

def test_small_obfuscated_bad(nserv, nzbget):
	nzb_content = nzbget.load_nzb('small-obfuscated.nzb')
	nzb_content = nzb_content.replace(';fsdkhKHGuwuMNBKskd', ';gpl.txt')
	hist = nzbget.download_nzb('small-obfuscated.mod.nzb', nzb_content, params=[('*naming', 'nzb')])
	assert hist['Status'] == 'SUCCESS/HEALTH'
	assert not os.path.exists(hist['DestDir'] + '/fsdkhKHGuwuMNBKskd')
	assert os.path.exists(hist['DestDir'] + '/gpl.txt')

def test_small(nserv, nzbget):
	hist = nzbget.download_nzb('small.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'
	assert os.path.exists(hist['DestDir'] + '/small.dat')

def test_small_changed(nserv, nzbget):
	nzb_content = nzbget.load_nzb('small.nzb')
	nzb_content = nzb_content.replace(';small.dat', ';small-changed.dat')
	hist = nzbget.download_nzb('small-changed.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/HEALTH'
	assert os.path.exists(hist['DestDir'] + '/small.dat')
