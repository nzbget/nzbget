nzbget_options = ['ParRename=yes']

def test_parrename(nserv, nzbget):
	hist = nzbget.download_nzb('parrename.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename3(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename5(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename5.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'
