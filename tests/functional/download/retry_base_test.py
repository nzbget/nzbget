nzbget_options = ['HealthCheck=park', 'ArticleCache=500', 'DirectWrite=yes']

def test_retry_1kb_redownload(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1k.dat.nzb')
	nzb_content = nzb_content.replace('1k.dat?3=6000:3000', '1k.dat?3=6000:3000!2')
	hist = nzbget.download_nzb('1k.bad2.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'
	nzbget.api.editserver(2, True)
	nzbget.api.editqueue('HistoryRedownload', 0, '', [hist['NZBID']])
	hist = nzbget.wait_nzb('1k.bad2.nzb')
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_retry_1kb_retryfailed(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1k.dat.nzb')
	nzb_content = nzb_content.replace('1k.dat?3=6000:3000', '1k.dat?3=6000:3000!2')
	hist = nzbget.download_nzb('1k.bad3.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'
	nzbget.api.editqueue('HistoryRetryFailed', 0, '', [hist['NZBID']])
	hist = nzbget.wait_nzb('1k.bad3.nzb')
	assert hist['Status'] == 'FAILURE/HEALTH'
	nzbget.api.editserver(2, True)
	nzbget.api.editqueue('HistoryRetryFailed', 0, '', [hist['NZBID']])
	hist = nzbget.wait_nzb('1k.bad3.nzb')
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_retry_1gb_failed(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1gb.nzb')
	nzb_content = nzb_content.replace(':500000', ':500000!2')
	hist = nzbget.download_nzb('1gb.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'
	assert hist['DeleteStatus'] == 'HEALTH'

def test_retry_1gb_redownload(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1gb.nzb')
	nzb_content = nzb_content.replace(':500000', ':500000!2')
	hist = nzbget.download_nzb('1gb.bad.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'
	nzbget.api.editserver(2, True)
	nzbget.api.editqueue('HistoryRedownload', 0, '', [hist['NZBID']])
	hist = nzbget.wait_nzb('1gb.bad.nzb')
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_retry_1gb_retryfailed(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1gb.nzb')
	nzb_content = nzb_content.replace(':500000', ':500000!2')
	hist = nzbget.download_nzb('1gb.bad3.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'
	nzbget.api.editqueue('HistoryRetryFailed', 0, '', [hist['NZBID']])
	hist = nzbget.wait_nzb('1gb.bad3.nzb')
	assert hist['Status'] == 'FAILURE/HEALTH'
	nzbget.api.editserver(2, True)
	nzbget.api.editqueue('HistoryRetryFailed', 0, '', [hist['NZBID']])
	hist = nzbget.wait_nzb('1gb.bad3.nzb')
	nzbget.api.editserver(2, False)
	assert hist['Status'] == 'SUCCESS/HEALTH'
