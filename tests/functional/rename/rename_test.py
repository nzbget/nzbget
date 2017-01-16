nzbget_options = ['ParRename=yes', 'RarRename=yes', 'UnpackIgnoreExt=.cbr']

def test_parrename(nserv, nzbget):
	hist = nzbget.download_nzb('parrename.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_parrename_backup(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parrename.nzb')
	nzb_content = nzb_content.replace('parrename/parrename.par2?', 'parrename/parrename.par2.damaged?')
	hist = nzbget.download_nzb('parrename.backup.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'
	log = nzbget.api.loadlog(int(hist['ID']), 0, 1000)
	renamed = False
	for entry in log:
		print(entry['Text'])
		if entry['Text'].find('Successfully renamed') > -1 and entry['Text'].find('archive') == -1:
			renamed = True
	assert renamed == True


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

def test_rarrename_rar3ignoreext(nserv, nzbget):
	hist = nzbget.download_nzb('rar3ignoreext.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/HEALTH'
