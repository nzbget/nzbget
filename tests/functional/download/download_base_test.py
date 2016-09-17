nzbget_options = ['HealthCheck=none', 'ArticleCache=500', 'DirectWrite=yes']

def test_1kb(nserv, nzbget):
	hist = nzbget.download_nzb('1k.dat.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_1kb_bad(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1k.dat.nzb')
	nzb_content = nzb_content.replace('1k.dat?3=6000:3000', '1k.dat?3=6000:3000!0')
	hist = nzbget.download_nzb('1k.dat.bad.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'

def test_1gb(nserv, nzbget):
	hist = nzbget.download_nzb('1gb.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_1gb_unpack(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1gb.nzb')
	hist = nzbget.download_nzb('1gb_unpack.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_9gb(nserv, nzbget):
	hist = nzbget.download_nzb('9gb.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_oneserver_1kb_success(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1k.dat.nzb')
	nzb_content = nzb_content.replace('1k.dat?3=6000:3000', '1k.dat?3=6000:3000!1')
	hist = nzbget.download_nzb('1k.success.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_oneserver_1kb_bad(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1k.dat.nzb')
	nzb_content = nzb_content.replace('1k.dat?3=6000:3000', '1k.dat?3=6000:3000!2')
	hist = nzbget.download_nzb('1k.bad2.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'

def test_twoservers_1kb_success(nserv, nzbget):
	nzbget.api.editserver(2, True)
	nzb_content = nzbget.load_nzb('1k.dat.nzb')
	nzb_content = nzb_content.replace('1k.dat?3=6000:3000', '1k.dat?3=6000:3000!2')
	hist = nzbget.download_nzb('1k.twoservers.nzb', nzb_content)
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_oneserver_1gb_failed(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1gb.nzb')
	nzb_content = nzb_content.replace(':500000', ':500000!2')
	hist = nzbget.download_nzb('1gb-health-failed.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'

def test_twoservers_1gb_success(nserv, nzbget):
	nzbget.api.editserver(2, True)
	nzb_content = nzbget.load_nzb('1gb.nzb')
	nzb_content = nzb_content.replace(':500000', ':500000!2')
	hist = nzbget.download_nzb('1gb-health-two.nzb', nzb_content)
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'
