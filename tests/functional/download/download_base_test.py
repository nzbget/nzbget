nzbget_options = ['HealthCheck=none', 'ArticleCache=500', 'DirectWrite=yes']

def test_small(nserv, nzbget):
	hist = nzbget.download_nzb('small.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_small_bad(nserv, nzbget):
	nzb_content = nzbget.load_nzb('small.nzb')
	nzb_content = nzb_content.replace('?3=6000:3000', '?3=6000:3000!0')
	hist = nzbget.download_nzb('small.bad.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'

def test_medium(nserv, nzbget):
	hist = nzbget.download_nzb('medium.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_medium_unpack(nserv, nzbget):
	nzb_content = nzbget.load_nzb('medium.nzb')
	hist = nzbget.download_nzb('medium_unpack.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_large(nserv, nzbget):
	hist = nzbget.download_nzb('large.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_oneserver_small_success(nserv, nzbget):
	nzb_content = nzbget.load_nzb('small.nzb')
	nzb_content = nzb_content.replace('?3=6000:3000', '?3=6000:3000!1')
	hist = nzbget.download_nzb('small.success.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_oneserver_small_bad(nserv, nzbget):
	nzb_content = nzbget.load_nzb('small.nzb')
	nzb_content = nzb_content.replace('?3=6000:3000', '?3=6000:3000!2')
	hist = nzbget.download_nzb('small.bad2.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'

def test_twoservers_small_success(nserv, nzbget):
	nzbget.api.editserver(2, True)
	nzb_content = nzbget.load_nzb('small.nzb')
	nzb_content = nzb_content.replace('?3=6000:3000', '?3=6000:3000!2')
	hist = nzbget.download_nzb('small.twoservers.nzb', nzb_content)
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_oneserver_medium_failed(nserv, nzbget):
	nzb_content = nzbget.load_nzb('medium.nzb')
	nzb_content = nzb_content.replace(':500000', ':500000!2')
	hist = nzbget.download_nzb('medium-health-failed.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'

def test_twoservers_medium_success(nserv, nzbget):
	nzbget.api.editserver(2, True)
	nzb_content = nzbget.load_nzb('medium.nzb')
	nzb_content = nzb_content.replace(':500000', ':500000!2')
	hist = nzbget.download_nzb('medium-health-two.nzb', nzb_content)
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'
