nzbget_options = ['ParRename=yes', 'RarRename=yes', 'ParCheck=auto', 'DirectRename=no', 'ParScan=full']

def test_parjoin_1(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin1.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parjoin_2(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin2.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parjoin_3(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin3.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/PAR'
