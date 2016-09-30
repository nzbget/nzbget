nzbget_options = ['ParRename=yes', 'RarRename=yes']

def test_parrename(nserv, nzbget):
	hist = nzbget.download_nzb('parrename.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar3(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar5(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename5.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar3oldnam(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3oldnam.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar3badext(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3badext.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar5badext(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename5badext.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_2sets(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename2sets.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar3damaged(nserv, nzbget):
	nzb_content = nzbget.load_nzb('rarrename3sm.nzb')
	nzb_content = nzb_content.replace('abc.32?14=6500:500', 'abc.32?14=6500:500!2')
	hist = nzbget.download_nzb('rarrename3sm.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar3encdata(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3encdata.nzb', unpack=True, params=[('*unpack:password', '123')])
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar5encdata(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename5encdata.nzb', unpack=True, params=[('*unpack:password', '123')])
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar3encnam(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename3encnam.nzb', unpack=True, params=[('*unpack:password', '123')])
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rarrename_rar5encnam(nserv, nzbget):
	hist = nzbget.download_nzb('rarrename5encnam.nzb', unpack=True, params=[('*unpack:password', '123')])
	assert hist['Status'] == 'SUCCESS/UNPACK'
