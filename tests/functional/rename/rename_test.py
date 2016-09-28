nzbget_options = ['ParRename=yes', 'RarRename=yes']

def test_parrename(nserv, nzbget):
	hist = nzbget.download_nzb('parrename.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename3(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename5(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename5.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename3on(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3on.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename3badext(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3badext.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename5badext(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename5badext.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename2sets(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename2sets.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename3_damaged(nserv, nzbget):
	nzb_content = nzbget.load_nzb('rarrename3sm.nzb')
	nzb_content = nzb_content.replace('abc.32?14=6500:500', 'abc.32?14=6500:500!2')
	hist = nzbget.download_nzb('rarrename3sm.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'
